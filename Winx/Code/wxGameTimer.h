#pragma once

// PC own constructor/method/lifetime semantics on actual shared Timer/Queue.
// Header placement and API names are inferred; the portable object is not the
// native136-byte primary/secondary-interface layout recorded separately.
#include "Code/Sparkplug/spMessageQueue.h"
#include "Code/Sparkplug/spTimer.h"
#include "Analysis/Host/wxGameTimerHost.h"

namespace winx::reconstruction
{
    class wxGameTimer final : public sparkplug::reconstruction::spTaskTimer
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x78665312;

        explicit wxGameTimer(sparkplug::reconstruction::spTimer& globalClock,
            sparkplug::reconstruction::spMessageQueueHost* queueHost = nullptr) noexcept;
        ~wxGameTimer() override;
        wxGameTimer(const wxGameTimer&) = delete;
        wxGameTimer& operator=(const wxGameTimer&) = delete;

        static void SetFactoryBindingsForAnalysis(wxGameTimerFactoryBindingsForAnalysis) noexcept;
        [[nodiscard]] static wxGameTimer* GetInstanceForAnalysis() noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // Native table uses the literal root no-op, without copying runtime
        // time, own flag, queue, clock source or opaque base state.
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;

        [[nodiscard]] bool StartForAnalysis(const ClockSourceForAnalysis&,
            std::string* error = nullptr) override;
        void PauseForAnalysis() noexcept override;
        // Reset/Update are inherited. Reset does not clear this flag or Queue.

        [[nodiscard]] std::uint8_t GetPausedByteForAnalysis() const noexcept { return paused_; }
        // Literal setup of native byte40; no original setter name is claimed.
        void SetPausedByteForAnalysis(std::uint8_t value) noexcept { paused_ = value; }
        [[nodiscard]] sparkplug::reconstruction::spMessageQueue& QueueForAnalysis() noexcept { return queue_; }
        [[nodiscard]] const sparkplug::reconstruction::spMessageQueue& QueueForAnalysis() const noexcept { return queue_; }
        [[nodiscard]] sparkplug::reconstruction::spTimer& GlobalClockForAnalysis() noexcept { return globalClock_; }
        [[nodiscard]] const sparkplug::reconstruction::spTimer& GlobalClockForAnalysis() const noexcept { return globalClock_; }

    private:
        static wxGameTimer* instance_;
        sparkplug::reconstruction::spTimer& globalClock_;
        sparkplug::reconstruction::spMessageQueueHost* queueHost_;
        sparkplug::reconstruction::spMessageQueue queue_;
        std::uint8_t paused_ = 0;
    };
}
