#pragma once
// WinxCoop-owned Windows pump scheduling and observation. Relative waits yield
// the owner thread; QPC cadence metrics never replace protocol/lease clocks.
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace winxcoop::networking
{
    class WindowsPumpWait final
    {
        HANDLE timer_ = nullptr;
        bool period_ = false, highResolution_ = false;
        const bool preferHighResolution_;
        std::uint32_t created_ = 0, closed_ = 0, periodBegins_ = 0, periodEnds_ = 0;
        DWORD highResolutionError_ = 0, waitError_ = 0, closeError_ = 0;
        MMRESULT beginResult_ = TIMERR_NOERROR, endResult_ = TIMERR_NOERROR;
        std::uint64_t waits_ = 0, failures_ = 0;
    public:
        // false explicitly selects the scoped normal-timer branch for a real
        // ownership test. Production callers retain the preferred high-res path.
        explicit WindowsPumpWait(bool preferHighResolution = true)
            : preferHighResolution_(preferHighResolution)
        {
            // Unnamed, auto-reset, process-owned timer. Flag2 is the Windows
            // 10/1803 high-resolution option; fallback is measured, not a claim
            // that this diagnostic has been qualified on Windows7.
            if (preferHighResolution_)
            {
                timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0x2, TIMER_MODIFY_STATE | SYNCHRONIZE);
                if (timer_) { highResolution_ = true; ++created_; return; }
                highResolutionError_ = GetLastError();
                if (highResolutionError_ != ERROR_INVALID_PARAMETER && highResolutionError_ != ERROR_NOT_SUPPORTED &&
                    highResolutionError_ != ERROR_CALL_NOT_IMPLEMENTED && highResolutionError_ != ERROR_INVALID_FLAGS)
                    throw std::runtime_error("High-resolution pump timer creation failed for a non-compatibility reason");
            }
            beginResult_ = timeBeginPeriod(1);
            if (beginResult_ != TIMERR_NOERROR) throw std::runtime_error("Scoped pump timer resolution request failed");
            period_ = true; ++periodBegins_;
            timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_MODIFY_STATE | SYNCHRONIZE);
            if (!timer_) { waitError_ = GetLastError(); Close(); throw std::runtime_error("Fallback pump waitable timer creation failed"); }
            ++created_;
        }
        ~WindowsPumpWait() { Close(); }
        WindowsPumpWait(const WindowsPumpWait&) = delete;
        WindowsPumpWait& operator=(const WindowsPumpWait&) = delete;
        bool WaitOneMillisecond() noexcept
        {
            ++waits_; LARGE_INTEGER due{}; due.QuadPart = -10000; // Relative100ns units, one-shot1ms.
            if (!timer_) { waitError_ = ERROR_INVALID_HANDLE; ++failures_; return false; }
            if (!SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE))
            { waitError_ = GetLastError(); ++failures_; return false; }
            const auto result = WaitForSingleObject(timer_, 100);
            if (result != WAIT_OBJECT_0)
            { waitError_ = result == WAIT_FAILED ? GetLastError() : result; ++failures_; return false; }
            return true;
        }
        void Close() noexcept
        {
            if (timer_)
            {
                (void)CancelWaitableTimer(timer_);
                if (CloseHandle(timer_)) ++closed_; else closeError_ = GetLastError();
                timer_ = nullptr;
            }
            if (period_)
            {
                endResult_ = timeEndPeriod(1); if (endResult_ == TIMERR_NOERROR) ++periodEnds_;
                period_ = false;
            }
        }
        bool Balanced() const noexcept { return created_ == closed_ && periodBegins_ == periodEnds_ && closeError_ == 0; }
        std::string Json() const
        {
            return "{\"mode\":\"" + std::string(highResolution_ ? "high-resolution-waitable-timer" : "scoped-period-normal-timer") +
                "\",\"timerHandlesCreated\":" + std::to_string(created_) + ",\"timerHandlesClosed\":" + std::to_string(closed_) +
                ",\"periodBeginCount\":" + std::to_string(periodBegins_) + ",\"periodEndCount\":" + std::to_string(periodEnds_) +
                ",\"highResolutionCreateError\":" + std::to_string(highResolutionError_) + ",\"periodBeginResult\":" + std::to_string(beginResult_) +
                ",\"periodEndResult\":" + std::to_string(endResult_) + ",\"waitCalls\":" + std::to_string(waits_) +
                ",\"waitFailures\":" + std::to_string(failures_) + ",\"waitError\":" + std::to_string(waitError_) +
                ",\"closeError\":" + std::to_string(closeError_) + ",\"balanced\":" + (Balanced() ? "true" : "false") +
                ",\"highResolutionRequested\":" + (preferHighResolution_ ? "true" : "false") + ",\"win7RuntimeQualified\":false}";
        }
    };

    class WindowsPumpCadence final
    {
        std::uint64_t frequency_ = 0, last_ = 0, intervals_ = 0, sum_ = 0, maximum_ = 0;
        std::array<std::uint64_t, 5> histogram_{};
    public:
        WindowsPumpCadence()
        {
            LARGE_INTEGER frequency{};
            if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
                throw std::runtime_error("Pump QPC frequency unavailable");
            frequency_ = static_cast<std::uint64_t>(frequency.QuadPart);
        }
        bool Observe() noexcept
        {
            LARGE_INTEGER value{};
            if (!QueryPerformanceCounter(&value) || value.QuadPart < 0) return false;
            const auto tick = static_cast<std::uint64_t>(value.QuadPart);
            if (last_)
            {
                if (tick < last_) return false;
                const auto delta = tick - last_; ++intervals_; sum_ += delta;
                maximum_ = (std::max)(maximum_, delta);
                const auto micros = static_cast<std::uint64_t>(static_cast<long double>(delta) * 1000000 / frequency_);
                ++histogram_[micros <= 2000 ? 0 : micros <= 5000 ? 1 : micros <= 10000 ? 2 : micros <= 20000 ? 3 : 4];
            }
            last_ = tick;
            return true;
        }
        // Flat JSON fields retain the original InputBridge report contract.
        std::string JsonFields() const
        {
            return "\"pumpIntervals\":" + std::to_string(intervals_) + ",\"pumpAverageMicroseconds\":" +
                std::to_string(intervals_ ? static_cast<double>(static_cast<long double>(sum_) * 1000000 / frequency_ / intervals_) : 0.0) +
                ",\"pumpMaximumMicroseconds\":" + std::to_string(static_cast<std::uint64_t>(static_cast<long double>(maximum_) * 1000000 / frequency_)) +
                ",\"pumpHistogramLe2Le5Le10Le20Gt20Ms\":[" + std::to_string(histogram_[0]) + "," + std::to_string(histogram_[1]) + "," +
                std::to_string(histogram_[2]) + "," + std::to_string(histogram_[3]) + "," + std::to_string(histogram_[4]) + "]";
        }
    };
}
