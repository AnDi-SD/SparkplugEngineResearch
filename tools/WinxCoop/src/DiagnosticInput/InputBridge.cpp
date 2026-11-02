// Finite diagnostic native-input delivery bridge. Not the P launcher/runtime.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include "InputProtocol.h"
#include "Networking/WindowsTcpTransport.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <deque>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

using namespace winxcoop::diagnostic_input;
using namespace winxcoop::networking;
namespace
{
    std::uint64_t Number(const std::wstring& value, std::uint64_t maximum)
    {
        std::string ascii;
        for(auto c : value) { if(c < L'0' || c > L'9') throw std::invalid_argument("Expected decimal integer"); ascii += static_cast<char>(c); }
        std::uint64_t result = 0;
        const auto parsed = std::from_chars(ascii.data(), ascii.data() + ascii.size(), result);
        if(ascii.empty() || parsed.ec != std::errc{} || parsed.ptr != ascii.data() + ascii.size() || result > maximum)
            throw std::invalid_argument("Integer outside diagnostic bound");
        return result;
    }
    Endpoint EndpointValue(const std::wstring& value)
    {
        const auto colon = value.rfind(L':');
        if(colon == std::wstring::npos) throw std::invalid_argument("Endpoint must be IPv4:port");
        const auto ip = value.substr(0, colon); std::size_t begin = 0; std::uint32_t address = 0;
        for(unsigned i = 0; i < 4; ++i)
        {
            const auto end = i == 3 ? ip.size() : ip.find(L'.', begin);
            if(end == std::wstring::npos || end <= begin) throw std::invalid_argument("Invalid IPv4");
            address = (address << 8) | static_cast<std::uint32_t>(Number(ip.substr(begin, end - begin), 255)); begin = end + 1;
        }
        return {address, static_cast<std::uint16_t>(Number(value.substr(colon + 1), 65535))};
    }
    std::uint64_t Creation(HANDLE process)
    {
        FILETIME created{}, exited{}, kernel{}, user{};
        if(!GetProcessTimes(process, &created, &exited, &kernel, &user)) throw std::runtime_error("Cannot verify process creation identity");
        return (std::uint64_t(created.dwHighDateTime) << 32) | created.dwLowDateTime;
    }
    void FreshFile(const std::filesystem::path& path, const std::string& data)
    {
        if(path.empty()) return;
        const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if(file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create fresh diagnostic output");
        DWORD written = 0;
        const bool ok = data.size() <= MAXDWORD && WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) && written == data.size();
        CloseHandle(file); if(!ok) throw std::runtime_error("Cannot write diagnostic output");
    }
    struct Options
    {
        Role role = Role::Host;
        TcpTransportConfig tcp;
        std::wstring mapping;
        std::filesystem::path ready, report;
        std::uint64_t nonce = 0, session = 0, scenario = 0, runMilliseconds = 60000;
        std::uint32_t levelEpoch = 0, generation = 0, credits = MaximumCredits, maxSamples = 4096, actorLeaseMilliseconds = 500;
        std::uint16_t token = 0xE404;
    };
    Options Parse(int argc, wchar_t** argv)
    {
        Options o; bool role = false, bind = false;
        for(int i = 1; i < argc; ++i)
        {
            const std::wstring key = argv[i];
            if(i + 1 >= argc) throw std::invalid_argument("Every option requires a value");
            const std::wstring value = argv[++i];
            if(key == L"--role")
            {
                if(value != L"host" && value != L"guest") throw std::invalid_argument("Role is host or guest");
                o.role = value == L"host" ? Role::Host : Role::Guest;
                o.tcp.role = o.role == Role::Host ? SessionRole::Host : SessionRole::Guest; role = true;
            }
            else if(key == L"--mapping") o.mapping = value;
            else if(key == L"--bind") { o.tcp.localBind = EndpointValue(value); bind = true; }
            else if(key == L"--server") o.tcp.server = EndpointValue(value);
            else if(key == L"--nonce") o.nonce = Number(value, UINT64_MAX);
            else if(key == L"--session") o.session = Number(value, UINT64_MAX);
            else if(key == L"--scenario") o.scenario = Number(value, UINT64_MAX);
            else if(key == L"--level-epoch") o.levelEpoch = static_cast<std::uint32_t>(Number(value, UINT32_MAX));
            else if(key == L"--generation") o.generation = static_cast<std::uint32_t>(Number(value, UINT32_MAX));
            else if(key == L"--credits") o.credits = static_cast<std::uint32_t>(Number(value, MaximumCredits));
            else if(key == L"--run-ms") o.runMilliseconds = Number(value, 600000);
            else if(key == L"--max-samples") o.maxSamples = static_cast<std::uint32_t>(Number(value, 65536));
            else if(key == L"--actor-lease-ms") o.actorLeaseMilliseconds = static_cast<std::uint32_t>(Number(value, 5000));
            else if(key == L"--token") o.token = static_cast<std::uint16_t>(Number(value, 65535));
            else if(key == L"--ready-file") o.ready = value;
            else if(key == L"--report") o.report = value;
            else throw std::invalid_argument("Unknown diagnostic option");
        }
        const std::wstring prefix = L"Local\\WinxCoop.DiagnosticInput5.";
        if(!role || !bind || !o.nonce || !o.session || !o.scenario || !o.levelEpoch || !o.generation ||
            !o.credits || !o.maxSamples || !o.token || o.runMilliseconds < 100 || o.actorLeaseMilliseconds < 500 ||
            o.mapping.size() <= prefix.size() || o.mapping.size() > 160 || o.mapping.compare(0, prefix.size(), prefix) != 0 ||
            (o.role == Role::Guest) != o.tcp.server.has_value()) throw std::invalid_argument("Missing/invalid explicit diagnostic identity or endpoint");
        for(std::size_t i = prefix.size(); i < o.mapping.size(); ++i)
        {
            const auto c = o.mapping[i];
            if(!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'.' || c == L'-' || c == L'_'))
                throw std::invalid_argument("Mapping suffix has invalid character");
        }
        return o;
    }
    class Mapping final
    {
        HANDLE handle_ = nullptr;
        Shared* shared_ = nullptr;
    public:
        explicit Mapping(const Options& o)
        {
            const auto ownerCreation = Creation(GetCurrentProcess());
            handle_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(Shared), o.mapping.c_str());
            if(!handle_) throw std::runtime_error("Cannot create diagnostic mapping");
            if(GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(handle_); handle_ = nullptr; throw std::runtime_error("Diagnostic mapping must be fresh"); }
            shared_ = static_cast<Shared*>(MapViewOfFile(handle_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
            if(!shared_) { CloseHandle(handle_); handle_ = nullptr; throw std::runtime_error("Cannot map diagnostic IPC"); }
            new(shared_) Shared{};
            shared_->header = {IpcMagic, IpcVersion, sizeof(Shared), RingCapacity, o.nonce, GetCurrentProcessId(),
                static_cast<std::uint32_t>(o.role), ownerCreation, o.session, o.scenario, o.levelEpoch, o.generation};
            MemoryBarrier();
        }
        ~Mapping() { if(shared_) UnmapViewOfFile(shared_); if(handle_) CloseHandle(handle_); }
        Mapping(const Mapping&) = delete;
        Mapping& operator=(const Mapping&) = delete;
        Shared& Get() noexcept { return *shared_; }
    };
    class PumpWait final
    {
        HANDLE timer_ = nullptr;
        bool period_ = false, highResolution_ = false;
        std::uint32_t created_ = 0, closed_ = 0, periodBegins_ = 0, periodEnds_ = 0;
        DWORD highResolutionError_ = 0, waitError_ = 0, closeError_ = 0;
        MMRESULT beginResult_ = TIMERR_NOERROR, endResult_ = TIMERR_NOERROR;
        std::uint64_t waits_ = 0, failures_ = 0;
    public:
        PumpWait()
        {
            // Unnamed, auto-reset, process-owned timer. Flag2 is the Windows
            // 10/1803 high-resolution option; its absence is a measured fallback,
            // never a claim that this diagnostic has been qualified on Win7.
            timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0x2, TIMER_MODIFY_STATE | SYNCHRONIZE);
            if(timer_) { highResolution_ = true; ++created_; return; }
            highResolutionError_ = GetLastError();
            if(highResolutionError_ != ERROR_INVALID_PARAMETER && highResolutionError_ != ERROR_NOT_SUPPORTED &&
                highResolutionError_ != ERROR_CALL_NOT_IMPLEMENTED && highResolutionError_ != ERROR_INVALID_FLAGS)
                throw std::runtime_error("High-resolution pump timer creation failed for a non-compatibility reason");
            beginResult_ = timeBeginPeriod(1);
            if(beginResult_ != TIMERR_NOERROR) throw std::runtime_error("Scoped pump timer resolution request failed");
            period_ = true; ++periodBegins_;
            timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_MODIFY_STATE | SYNCHRONIZE);
            if(!timer_) { waitError_ = GetLastError(); Close(); throw std::runtime_error("Fallback pump waitable timer creation failed"); }
            ++created_;
        }
        ~PumpWait() { Close(); }
        PumpWait(const PumpWait&) = delete;
        PumpWait& operator=(const PumpWait&) = delete;
        bool WaitOneMillisecond() noexcept
        {
            ++waits_; LARGE_INTEGER due{}; due.QuadPart = -10000; // Relative100ns units, one-shot1ms.
            if(!timer_) { waitError_ = ERROR_INVALID_HANDLE; ++failures_; return false; }
            if(!SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE))
            { waitError_ = GetLastError(); ++failures_; return false; }
            const auto result = WaitForSingleObject(timer_, 100);
            if(result != WAIT_OBJECT_0)
            { waitError_ = result == WAIT_FAILED ? GetLastError() : result; ++failures_; return false; }
            return true;
        }
        void Close() noexcept
        {
            if(timer_)
            {
                (void)CancelWaitableTimer(timer_);
                if(CloseHandle(timer_)) ++closed_; else closeError_ = GetLastError();
                timer_ = nullptr;
            }
            if(period_)
            {
                endResult_ = timeEndPeriod(1); if(endResult_ == TIMERR_NOERROR) ++periodEnds_;
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
                ",\"win7RuntimeQualified\":false}";
        }
    };
    class Bridge final
    {
        struct Grant { std::uint32_t id, maximum; std::uint64_t deadline; };
        Options o_;
        Mapping mapping_;
        Shared& ipc_;
        const Header header_;
        WindowsTcpTransport tcp_;
        PumpWait wait_;
        HANDLE actorProcess_ = nullptr;
        ActorStatus actor_{};
        BridgeStatus status_{};
        std::optional<Message> control_;
        std::deque<Grant> grants_;
        std::deque<InputRecord> pending_;
        bool hello_ = false, sourceReady_ = false, haveActor_ = false, sourceNeutral_ = true;
        std::uint32_t sourceRevision_ = 0, actorRevision_ = 0, epoch_ = 0, nextGrant_ = 0;
        std::uint32_t received_ = 0, forwarded_ = 0, acknowledged_ = 0, advertisedAck_ = 0, guestSent_ = 0, guestAck_ = 0;
        std::uint64_t connection_ = 0, token_ = 0, lastNow_ = 0, lastGrantAt_ = 0, lastIngress_ = 0, lastOrdinal_ = 0, lastCapture_ = 0;
        std::uint64_t receivedTotal_ = 0, sentTotal_ = 0, appliedTotal_ = 0, staleEpochs_ = 0, rearms_ = 0, ringWaits_ = 0;
        Terminal terminal_ = Terminal::None;
        std::uint64_t qpcFrequency_ = 0, lastPumpQpc_ = 0, pumpIntervals_ = 0, pumpTickSum_ = 0, pumpTickMaximum_ = 0;
        std::array<std::uint64_t, 5> pumpHistogram_{}; // <=2ms, <=5ms, <=10ms, <=20ms, >20ms.
        static constexpr std::size_t TraceCapacity = 1024 * 1024;
        std::string trace_;
        std::uint64_t traceLines_ = 0, droppedDiagnosticLines_ = 0;
        bool traceFlushAttempted_ = false, traceFlushedAfterStop_ = false;

        bool Host() const noexcept { return o_.role == Role::Host; }
        void Trace(const std::string& line)
        {
            if(line.size() > TraceCapacity - trace_.size()) { ++droppedDiagnosticLines_; return; }
            trace_.append(line); ++traceLines_;
        }
        void FlushTraceAfterStop()
        {
            if(o_.report.empty() || traceFlushAttempted_) return;
            traceFlushAttempted_ = true;
            auto path = o_.report; path += L".trace.jsonl";
            FreshFile(path, trace_); traceFlushedAfterStop_ = true;
        }
        Message Base(Kind kind) const
        {
            Message m; m.kind = kind; m.sessionId = o_.session; m.scenarioId = o_.scenario;
            m.connectionToken = token_; m.levelEpoch = o_.levelEpoch; m.bodyGeneration = o_.generation; m.inputEpoch = epoch_; return m;
        }
        bool Send(const Message& m, std::uint64_t now)
        {
            const auto bytes = Encode(m);
            if(!bytes) { Stop(Terminal::InvalidRecord); return false; }
            return tcp_.SendApplicationMessage(connection_, ApplicationClass, static_cast<std::uint32_t>(m.kind), bytes->data(), WireBytes, now);
        }
        void Stop(Terminal why)
        {
            if(terminal_ != Terminal::None) return;
            terminal_ = why; status_.terminalReason = static_cast<std::uint32_t>(why);
            status_.state = static_cast<std::uint32_t>(BridgeState::Closed); status_.neutralRequired = 1;
            if(Host()) Latch(ipc_.input.producerTerminal, why);
            Trace("{\"event\":\"input4-terminal\",\"reason\":" + std::to_string(static_cast<unsigned>(why)) + "}\n");
        }
        void ActorLeaseExpired(const char* phase, const ActorStatus& status, std::uint64_t now, std::uint64_t readNow)
        {
            Trace("{\"event\":\"input4-actor-lease-expired\",\"phase\":\"" + std::string(phase) + "\",\"observedAt\":" + std::to_string(status.observedAtMilliseconds) +
                ",\"now\":" + std::to_string(now) + ",\"readNow\":" + std::to_string(readNow) + ",\"age\":" + std::to_string(readNow - status.observedAtMilliseconds) +
                ",\"actorLeaseMillis\":" + std::to_string(o_.actorLeaseMilliseconds) + ",\"participantPid\":" + std::to_string(status.participantPid) +
                ",\"readinessRevision\":" + std::to_string(status.readinessRevision) + ",\"publication\":" + std::to_string(status.publication) + "}\n");
            Stop(Terminal::LeaseExpired);
        }
        bool ReadActor(std::uint64_t now)
        {
            // Status can remain odd, unreadable, or temporarily zero while a
            // writer stalls. Such a read must never renew cached Ready. Verify
            // the previously admitted process/lease before the first early
            // return, independently of whether a fresh publication is readable.
            if(haveActor_)
            {
                if(!actorProcess_ || WaitForSingleObject(actorProcess_, 0) != WAIT_TIMEOUT)
                { Stop(Terminal::ActorClosed); return false; }
                if(actor_.observedAtMilliseconds > now) { Stop(Terminal::Clock); return false; }
                if(actor_.availability == static_cast<std::uint32_t>(Availability::Ready) &&
                    now - actor_.observedAtMilliseconds > o_.actorLeaseMilliseconds)
                { ActorLeaseExpired("cached-before-read", actor_, now, now); return false; }
            }
            ActorStatus current;
            if(!ReadStatus(ipc_.actor, current)) return false;
            if(!current.participantPid) return false;
            // The other process can publish after the outer Pump timestamp was
            // sampled. Validate its local clock against a time taken after the
            // copy; the fixed network ingress timestamp remains the Pump time.
            const auto readNow = GetTickCount64();
            if(readNow < now) { Stop(Terminal::Clock); return false; }
            if(current.reserved0 || current.reserved1 || current.reserved2 ||
                current.availability > static_cast<std::uint32_t>(Availability::Retired) || !current.readinessRevision ||
                current.observedAtMilliseconds > readNow || current.terminalReason > static_cast<std::uint32_t>(Terminal::NativeSource))
            { Stop(Terminal::Identity); return false; }
            if(!haveActor_)
            {
                actorProcess_ = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, current.participantPid);
                if(!actorProcess_ || Creation(actorProcess_) != current.participantCreationFileTime) { Stop(Terminal::Identity); return false; }
                haveActor_ = true;
            }
            else if(current.participantPid != actor_.participantPid || current.participantCreationFileTime != actor_.participantCreationFileTime ||
                current.readinessRevision < actor_.readinessRevision || current.observedAtMilliseconds < actor_.observedAtMilliseconds)
            { Stop(Terminal::Identity); return false; }
            if(haveActor_ && actor_.participantPid && current.availability != actor_.availability && current.readinessRevision == actor_.readinessRevision)
            { Stop(Terminal::Sequence); return false; }
            if(WaitForSingleObject(actorProcess_, 0) != WAIT_TIMEOUT) { Stop(Terminal::ActorClosed); return false; }
            actor_ = current;
            if(current.terminalReason) { Stop(static_cast<Terminal>(current.terminalReason)); return false; }
            if(current.availability == static_cast<std::uint32_t>(Availability::Retired)) { Stop(Terminal::ActorClosed); return false; }
            if(current.availability == static_cast<std::uint32_t>(Availability::Ready) && readNow - current.observedAtMilliseconds > o_.actorLeaseMilliseconds)
            { ActorLeaseExpired("fresh-status", current, now, readNow); return false; }
            return true;
        }
        bool ActorReady() const noexcept
        { return haveActor_ && actor_.availability == static_cast<std::uint32_t>(Availability::Ready); }
        bool Running() const noexcept
        { return tcp_.Session() && tcp_.Session()->Phase() == SessionPhase::Running; }
        void ObservePumpInterval()
        {
            LARGE_INTEGER value{};
            if(!QueryPerformanceCounter(&value) || value.QuadPart < 0) { Stop(Terminal::Clock); return; }
            const auto tick = static_cast<std::uint64_t>(value.QuadPart);
            if(lastPumpQpc_)
            {
                if(tick < lastPumpQpc_) { Stop(Terminal::Clock); return; }
                const auto delta = tick - lastPumpQpc_; ++pumpIntervals_; pumpTickSum_ += delta;
                pumpTickMaximum_ = (std::max)(pumpTickMaximum_, delta);
                const auto micros = static_cast<std::uint64_t>(static_cast<long double>(delta) * 1000000 / qpcFrequency_);
                ++pumpHistogram_[micros <= 2000 ? 0 : micros <= 5000 ? 1 : micros <= 10000 ? 2 : micros <= 20000 ? 3 : 4];
            }
            lastPumpQpc_ = tick;
        }
        void Pause()
        {
            advertisedAck_ = 0;
            control_.reset(); pending_.clear(); grants_.clear();
            status_.state = static_cast<std::uint32_t>(BridgeState::Paused); status_.neutralRequired = 1;
            status_.grant = 0; status_.maximumSourceSequence = 0; status_.leaseDeadlineMilliseconds = 0;
            if(Host() && hello_) { auto m = Base(Kind::Pause); m.flags = NeutralRequired; control_ = m; }
        }
        void Rearm(std::uint64_t now)
        {
            if(epoch_ == UINT32_MAX) { Stop(Terminal::CounterExhausted); return; }
            ++epoch_; ++rearms_; received_ = forwarded_ = acknowledged_ = advertisedAck_ = 0; sourceNeutral_ = true;
            pending_.clear(); grants_.clear(); control_.reset(); lastIngress_ = now; lastGrantAt_ = 0;
            status_.inputEpoch = epoch_; status_.acknowledgedSourceSequence = 0;
            status_.neutralRequired = 1; status_.state = static_cast<std::uint32_t>(BridgeState::Paused);
            status_.grant = 0; status_.maximumSourceSequence = 0; status_.leaseDeadlineMilliseconds = 0;
            Trace("{\"event\":\"input4-epoch\",\"epoch\":" + std::to_string(epoch_) + ",\"now\":" + std::to_string(now) + "}\n");
        }
        void Publish(std::uint64_t now)
        {
            status_.connectionToken = token_; status_.bridgeTickMilliseconds = now;
            if(!PublishStatus(ipc_.bridge, status_)) Stop(Terminal::CounterExhausted);
        }
        void ValidateHeader()
        {
            if(std::memcmp(&ipc_.header, &header_, sizeof(Header)) != 0) Stop(Terminal::Identity);
            const auto failure = Acquire(ipc_.input.producerTerminal);
            if(failure) Stop(failure <= static_cast<unsigned>(Terminal::NativeSource) ? static_cast<Terminal>(failure) : Terminal::InvalidRecord);
            const auto write = Acquire(ipc_.input.writeCursor), read = Acquire(ipc_.input.readCursor);
            if(write < read || write - read > RingCapacity || ipc_.input.reserved ||
                std::any_of(std::begin(ipc_.input.reservedHeader), std::end(ipc_.input.reservedHeader), [](auto n) { return n != 0; })) Stop(Terminal::InvalidRecord);
        }
        bool Identity(const Message& m) const noexcept
        { return m.sessionId == o_.session && m.scenarioId == o_.scenario && m.levelEpoch == o_.levelEpoch && m.bodyGeneration == o_.generation; }
        void ReceiveHost(const Message& m, std::uint64_t now)
        {
            if(m.kind == Kind::Hello)
            {
                if(hello_) { Stop(Terminal::Sequence); return; }
                hello_ = true; sourceReady_ = m.flags != 0; sourceRevision_ = m.sourceRevision;
                actorRevision_ = actor_.readinessRevision;
                if(ActorReady() && sourceReady_) Rearm(now); else Pause(); return;
            }
            if(!hello_ || m.connectionToken != token_) { Stop(Terminal::Identity); return; }
            if(m.kind == Kind::Stop) { Stop(static_cast<Terminal>(m.terminalReason)); return; }
            if(m.kind == Kind::SourceState)
            {
                if(m.sourceRevision <= sourceRevision_) { Stop(Terminal::Sequence); return; }
                sourceRevision_ = m.sourceRevision; sourceReady_ = m.flags != 0;
                if(sourceReady_ && ActorReady()) Rearm(now); else Pause(); return;
            }
            if(m.kind != Kind::Input) { Stop(Terminal::InvalidRecord); return; }
            if(m.inputEpoch < epoch_) { ++staleEpochs_; return; }
            if(m.inputEpoch != epoch_) { Stop(Terminal::Identity); return; }
            if(!ActorReady() || !sourceReady_ || status_.state != static_cast<unsigned>(BridgeState::Ready)) { ++staleEpochs_; return; }
            const auto grant = std::find_if(grants_.begin(), grants_.end(), [&](const Grant& g) { return g.id == m.grant; });
            if(grant == grants_.end() || grant->deadline != m.leaseDeadlineMilliseconds || now > grant->deadline)
            { Rearm(now); return; }
            const auto& r = m.input;
            if(r.sourceSequence != received_ + 1 || r.sourceSequence > grant->maximum || r.captureOrdinal <= lastOrdinal_ ||
                (sourceNeutral_ && r.buttons != 0) || pending_.size() >= MaximumCredits || receivedTotal_ == o_.maxSamples)
            { Stop(Terminal::Sequence); return; }
            InputRecord accepted = r; accepted.hostIngressMilliseconds = now; // Assigned exactly once at this Pump's decode admission.
            pending_.push_back(accepted); received_ = r.sourceSequence; lastOrdinal_ = r.captureOrdinal; lastIngress_ = now;
            sourceNeutral_ = false; ++receivedTotal_;
            Trace("{\"event\":\"input4-ingress\",\"epoch\":" + std::to_string(epoch_) + ",\"sequence\":" + std::to_string(received_) +
                ",\"captureOrdinal\":" + std::to_string(r.captureOrdinal) + ",\"hostIngress\":" + std::to_string(now) + "}\n");
        }
        void ReceiveGuest(const Message& m)
        {
            if(m.kind == Kind::Hello || m.kind == Kind::Input || m.kind == Kind::SourceState || !m.connectionToken)
            { Stop(Terminal::InvalidRecord); return; }
            if(token_ && token_ != m.connectionToken) { Stop(Terminal::Identity); return; }
            token_ = m.connectionToken;
            if(m.kind == Kind::Stop) { Stop(static_cast<Terminal>(m.terminalReason)); return; }
            if(m.kind == Kind::Pause) { Pause(); return; }
            if(m.inputEpoch < epoch_) { ++staleEpochs_; return; }
            if(m.kind == Kind::Grant)
            {
                if(m.inputEpoch > epoch_)
                {
                    epoch_ = m.inputEpoch; guestSent_ = guestAck_ = 0;
                    status_.grant = 0; status_.acknowledgedSourceSequence = 0;
                }
                if(m.grant <= status_.grant || m.acknowledgedSourceSequence > guestSent_ ||
                    m.acknowledgedSourceSequence < guestAck_) { Stop(Terminal::Sequence); return; }
                guestAck_ = m.acknowledgedSourceSequence;
                status_.inputEpoch = epoch_; status_.grant = m.grant; status_.leaseDeadlineMilliseconds = m.leaseDeadlineMilliseconds;
                status_.maximumSourceSequence = m.maximumSourceSequence; status_.acknowledgedSourceSequence = guestAck_;
                status_.neutralRequired = m.flags; status_.state = static_cast<unsigned>(BridgeState::Ready); return;
            }
            if(m.kind == Kind::Ack && m.inputEpoch == epoch_ && m.acknowledgedSourceSequence >= guestAck_ && m.acknowledgedSourceSequence <= guestSent_)
            { guestAck_ = m.acknowledgedSourceSequence; status_.acknowledgedSourceSequence = guestAck_; return; }
            Stop(Terminal::InvalidRecord);
        }
        void DrainAll(std::uint64_t now)
        {
            // Always drain immediately after Pump, including while the IPC ring
            // is full. Credits bound data backlog; no dequeue-time restamping.
            unsigned count = 0;
            while(auto received = tcp_.TakeMessage())
            {
                if(++count > WindowsTcpTransport::MaximumDeliveredMessages || received->connectionId != connection_)
                { Stop(Terminal::Identity); continue; }
                auto m = Decode(received->message);
                if(!m || !Identity(*m)) { Stop(Terminal::InvalidRecord); continue; }
                if(terminal_ != Terminal::None) continue;
                if(Host()) ReceiveHost(*m, now); else ReceiveGuest(*m);
            }
        }
        void HostWork(std::uint64_t now)
        {
            if(!hello_) return;
            if(actorRevision_ != actor_.readinessRevision)
            {
                actorRevision_ = actor_.readinessRevision;
                if(ActorReady() && sourceReady_) Rearm(now); else Pause();
            }
            if(!ActorReady() || !sourceReady_) return;
            if(!epoch_) Rearm(now);
            if(actor_.appliedEpoch > epoch_) { Stop(Terminal::Identity); return; }
            if(actor_.appliedEpoch == epoch_)
            {
                if(actor_.appliedSourceSequence < acknowledged_ || actor_.appliedSourceSequence > forwarded_)
                { Stop(Terminal::Sequence); return; }
                if(actor_.appliedSourceSequence > acknowledged_)
                {
                    appliedTotal_ += actor_.appliedSourceSequence - acknowledged_; acknowledged_ = actor_.appliedSourceSequence;
                    status_.acknowledgedSourceSequence = acknowledged_;
                }
            }
            if(now - lastIngress_ > LeaseMilliseconds) Rearm(now);
            if(!pending_.empty() && now - pending_.front().hostIngressMilliseconds > LeaseMilliseconds) Rearm(now);
            // At most8 unacknowledged samples. Old grant history has the same
            // credit ceiling; renewing its deadline does not mint more credit.
            const auto sinceGrant = now - lastGrantAt_;
            const bool ackProgress = acknowledged_ > advertisedAck_;
            if(!control_ && (!lastGrantAt_ || sinceGrant >= KeepaliveMilliseconds ||
                (ackProgress && sinceGrant >= MinimumGrantRenewalMilliseconds)))
            {
                if(nextGrant_ == UINT32_MAX || acknowledged_ > UINT32_MAX - o_.credits) { Stop(Terminal::CounterExhausted); return; }
                auto m = Base(Kind::Grant); m.grant = ++nextGrant_; m.leaseDeadlineMilliseconds = now + LeaseMilliseconds;
                m.acknowledgedSourceSequence = acknowledged_; m.maximumSourceSequence = acknowledged_ + o_.credits;
                m.flags = sourceNeutral_ ? NeutralRequired : 0; control_ = m;
            }
            if(control_)
            {
                if(control_->kind == Kind::Grant && now > control_->leaseDeadlineMilliseconds) { control_.reset(); return; }
                if(Send(*control_, now))
                {
                    if(control_->kind == Kind::Grant)
                    {
                        grants_.push_back({control_->grant, control_->maximumSourceSequence, control_->leaseDeadlineMilliseconds});
                        while(!grants_.empty() && grants_.front().deadline < now) grants_.pop_front();
                        if(grants_.size() > MaximumGrantHistory) { Stop(Terminal::Overflow); return; }
                        status_.grant = control_->grant; status_.leaseDeadlineMilliseconds = control_->leaseDeadlineMilliseconds;
                        status_.maximumSourceSequence = control_->maximumSourceSequence; status_.neutralRequired = control_->flags;
                        status_.state = static_cast<unsigned>(BridgeState::Ready); lastGrantAt_ = now;
                        advertisedAck_ = control_->acknowledgedSourceSequence;
                    }
                    control_.reset();
                }
            }
            while(!pending_.empty())
            {
                const auto write = Acquire(ipc_.input.writeCursor), read = Acquire(ipc_.input.readCursor);
                if(write == UINT32_MAX) { Stop(Terminal::CounterExhausted); return; }
                if(write - read == RingCapacity) { ++ringWaits_; break; }
                ipc_.input.records[write % RingCapacity] = pending_.front(); MemoryBarrier();
                Release(ipc_.input.writeCursor, write + 1); forwarded_ = pending_.front().sourceSequence; pending_.pop_front();
            }
        }
        void GuestWork(std::uint64_t now)
        {
            if(!haveActor_) return;
            if(!hello_)
            {
                auto m = Base(Kind::Hello); m.connectionToken = 0; m.inputEpoch = 0;
                m.sourceRevision = actor_.readinessRevision; m.flags = ActorReady() ? 1 : 0;
                if(Send(m, now)) { hello_ = true; actorRevision_ = actor_.readinessRevision; }
                return;
            }
            if(token_ && actorRevision_ != actor_.readinessRevision)
            {
                auto m = Base(Kind::SourceState); m.sourceRevision = actor_.readinessRevision; m.flags = ActorReady() ? 1 : 0;
                if(Send(m, now)) actorRevision_ = actor_.readinessRevision;
                else return;
            }
            if(!ActorReady() || status_.state != static_cast<unsigned>(BridgeState::Ready)) return;
            for(unsigned n = 0; n < MaximumCredits; ++n)
            {
                const auto write = Acquire(ipc_.input.writeCursor), read = Acquire(ipc_.input.readCursor);
                if(write == read) break;
                InputRecord r = ipc_.input.records[read % RingCapacity]; MemoryBarrier();
                if(r.inputEpoch < epoch_) { Release(ipc_.input.readCursor, read + 1); ++staleEpochs_; continue; }
                const auto captureNow = GetTickCount64();
                if(captureNow < now) { Stop(Terminal::Clock); return; }
                if(!ValidInput(r, true) || r.inputEpoch != epoch_ || r.sourceSequence != guestSent_ + 1 ||
                    r.sourceSequence > status_.maximumSourceSequence || r.captureOrdinal <= lastOrdinal_ ||
                    r.captureMilliseconds > captureNow || r.captureMilliseconds < lastCapture_ || sentTotal_ == o_.maxSamples)
                { Stop(Terminal::Sequence); return; }
                if(captureNow - r.captureMilliseconds > LeaseMilliseconds) { Stop(Terminal::LeaseExpired); return; }
                auto m = Base(Kind::Input); m.input = r; m.grant = r.grant; m.leaseDeadlineMilliseconds = r.leaseDeadlineMilliseconds;
                if(!Send(m, now)) break; // Cursor/record/sequence stay unchanged on transport Full.
                guestSent_ = r.sourceSequence; lastOrdinal_ = r.captureOrdinal; lastCapture_ = r.captureMilliseconds;
                ++sentTotal_; Release(ipc_.input.readCursor, read + 1);
                Trace("{\"event\":\"input4-send\",\"epoch\":" + std::to_string(epoch_) + ",\"sequence\":" + std::to_string(guestSent_) +
                    ",\"captureOrdinal\":" + std::to_string(r.captureOrdinal) + "}\n");
            }
        }
    public:
        explicit Bridge(Options options) : o_(std::move(options)), mapping_(o_), ipc_(mapping_.Get()), header_(ipc_.header), tcp_(o_.tcp)
        {
            trace_.reserve(TraceCapacity); // Allocate the bounded diagnostic store before publishing Ready.
            LARGE_INTEGER frequency{};
            if(!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) throw std::runtime_error("Pump QPC frequency unavailable");
            qpcFrequency_ = static_cast<std::uint64_t>(frequency.QuadPart);
            status_.state = static_cast<unsigned>(BridgeState::Waiting); status_.neutralRequired = 1; if(Host()) token_ = o_.nonce;
        }
        ~Bridge() { if(actorProcess_) CloseHandle(actorProcess_); }
        int Run() try
        {
            const auto begin = GetTickCount64(); lastNow_ = begin; Publish(begin);
            if(!tcp_.Start(begin) || (Host() && !tcp_.HostStart(o_.token, begin))) Stop(Terminal::Transport);
            const auto bound = tcp_.BoundEndpoint();
            FreshFile(o_.ready, "{\"pid\":" + std::to_string(header_.ownerPid) + ",\"creationFileTime\":" + std::to_string(header_.ownerCreationFileTime) +
                ",\"nonce\":" + std::to_string(header_.nonce) + ",\"ipcBytes\":" + std::to_string(sizeof(Shared)) +
                ",\"creationFileTimeDecimal\":\"" + std::to_string(header_.ownerCreationFileTime) + "\",\"nonceDecimal\":\"" + std::to_string(header_.nonce) +
                "\",\"port\":" + std::to_string(bound.port) + "}\n");
            while(terminal_ == Terminal::None)
            {
                const auto now = GetTickCount64();
                if(now < lastNow_) { Stop(Terminal::Clock); break; } lastNow_ = now;
                if(now - begin >= o_.runMilliseconds) { Stop(Terminal::RunLimit); break; }
                ValidateHeader(); if(terminal_ != Terminal::None) break;
                ReadActor(now); if(terminal_ != Terminal::None) break;
                ObservePumpInterval(); if(terminal_ != Terminal::None) break;
                tcp_.Pump(now);
                const auto active = tcp_.ActiveConnectionId();
                if(active)
                {
                    if(connection_ && connection_ != active) Stop(Terminal::Identity);
                    connection_ = active;
                    if(status_.state == static_cast<unsigned>(BridgeState::Waiting)) status_.state = static_cast<unsigned>(BridgeState::Connected);
                }
                DrainAll(now); // Same Pump timestamp, before IPC/work/backpressure.
                if(tcp_.State() == TcpTransportState::Failed || tcp_.State() == TcpTransportState::Closed ||
                    (connection_ && !active)) Stop(Terminal::PeerClosed);
                if(terminal_ == Terminal::None && Running()) { if(Host()) HostWork(now); else GuestWork(now); }
                if(terminal_ == Terminal::None && control_ && control_->kind == Kind::Pause && Send(*control_, now)) control_.reset();
                Publish(now);
                if(terminal_ == Terminal::None && !wait_.WaitOneMillisecond()) Stop(Terminal::Clock);
            }
            const auto end = GetTickCount64(); Publish(end);
            if(connection_ && token_ && Running()) { auto m = Base(Kind::Stop); m.terminalReason = static_cast<unsigned>(terminal_); (void)Send(m, end); tcp_.Pump(end); }
            tcp_.Close(); // Terminal status remains visible until both local views close.
            wait_.Close(); // Counts are final before the report; destructor also covers exceptional paths.
            FlushTraceAfterStop(); // No pipe/file output is performed by the running pump.
            const auto& metrics = tcp_.Metrics();
            const std::string report = "{\"diagnosticInput4\":true,\"terminalReason\":" + std::to_string(static_cast<unsigned>(terminal_)) +
                ",\"actorLeaseMillis\":" + std::to_string(o_.actorLeaseMilliseconds) +
                ",\"traceBytes\":" + std::to_string(trace_.size()) + ",\"traceLines\":" + std::to_string(traceLines_) +
                ",\"droppedDiagnosticLines\":" + std::to_string(droppedDiagnosticLines_) + ",\"traceFlushedAfterStop\":" + (traceFlushedAfterStop_ ? "true" : "false") +
                ",\"received\":" + std::to_string(receivedTotal_) + ",\"sent\":" + std::to_string(sentTotal_) +
                ",\"worldAppliedOrSuppressedAcknowledged\":" + std::to_string(appliedTotal_) + ",\"staleEpochs\":" + std::to_string(staleEpochs_) +
                ",\"rearms\":" + std::to_string(rearms_) + ",\"ringWaits\":" + std::to_string(ringWaits_) +
                ",\"grantsIssued\":" + std::to_string(nextGrant_) + ",\"lastAdvertisedAck\":" + std::to_string(advertisedAck_) +
                ",\"socketsOpened\":" + std::to_string(metrics.socketsOpened) + ",\"socketsClosed\":" + std::to_string(metrics.socketsClosed) +
                ",\"pumpIntervals\":" + std::to_string(pumpIntervals_) + ",\"pumpAverageMicroseconds\":" +
                std::to_string(pumpIntervals_ ? static_cast<double>(static_cast<long double>(pumpTickSum_) * 1000000 / qpcFrequency_ / pumpIntervals_) : 0.0) +
                ",\"pumpMaximumMicroseconds\":" + std::to_string(static_cast<std::uint64_t>(static_cast<long double>(pumpTickMaximum_) * 1000000 / qpcFrequency_)) +
                ",\"pumpHistogramLe2Le5Le10Le20Gt20Ms\":[" + std::to_string(pumpHistogram_[0]) + "," + std::to_string(pumpHistogram_[1]) + "," +
                std::to_string(pumpHistogram_[2]) + "," + std::to_string(pumpHistogram_[3]) + "," + std::to_string(pumpHistogram_[4]) +
                "],\"pumpWait\":" + wait_.Json() +
                ",\"wireCodec\":\"existing-SessionCore-Packet-MemoryStream\",\"attackQualified\":false,\"twoGameQualified\":false}\n";
            FreshFile(o_.report, report); std::cout << report;
            return wait_.Balanced() && (terminal_ == Terminal::RunLimit || terminal_ == Terminal::ActorClosed) ? 0 : 2;
        }
        catch(...)
        {
            Stop(Terminal::InvalidRecord); Publish(GetTickCount64()); tcp_.Close(); wait_.Close();
            try { FlushTraceAfterStop(); } catch(...) {} // Preserve the original failure; no retry of partial output.
            throw;
        }
    };
}
int wmain(int argc, wchar_t** argv)
{
    try { Bridge bridge(Parse(argc, argv)); return bridge.Run(); }
    catch(const std::exception& error) { std::cerr << "InputBridge: " << error.what() << '\n'; return 1; }
}
