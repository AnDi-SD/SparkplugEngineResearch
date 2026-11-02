#include "../src/DiagnosticInput/InputProtocol.h"
#ifdef _WIN32
#include "../src/Networking/WindowsTcpTransport.h"
#endif
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

using namespace winxcoop::diagnostic_input;
using winxcoop::networking::SessionMessage;
namespace
{
    void Check(bool pass, const char* description) { if(!pass) throw std::runtime_error(description); }
    Message Base(Kind kind)
    {
        Message m; m.kind = kind; m.sessionId = 0x0102030405060708ull; m.scenarioId = 29;
        m.connectionToken = 71; m.levelEpoch = 3; m.bodyGeneration = 4; m.inputEpoch = 5; return m;
    }
    SessionMessage Carrier(const Message& m)
    {
        auto payload = Encode(m); Check(payload.has_value(), "valid diagnostic envelope encodes");
        return {ApplicationClass, static_cast<std::uint32_t>(m.kind), std::move(*payload)};
    }
    Message Grant()
    {
        auto m = Base(Kind::Grant); m.grant = 6; m.leaseDeadlineMilliseconds = 1500;
        m.maximumSourceSequence = 8; m.flags = NeutralRequired; return m;
    }
    Message Input()
    {
        auto m = Base(Kind::Input); m.grant = 6; m.leaseDeadlineMilliseconds = 1500;
        m.input = {5, 1, 20, 1000, 0, 1, -1.5f, 2.5f, 1, 6, 0, 1500}; return m;
    }
    void RoundTripAndCarrierBoundaries()
    {
        auto m = Input(); auto wire = Carrier(m);
        Check(wire.payload.size() == WireBytes && WireBytes == 140, "exact scalar payload size independent of host padding");
        // Independent little-endian scalar fixture, not another packet codec.
        const unsigned char prefix[] = {5,0,0,0,3,0,0,0,8,7,6,5,4,3,2,1};
        Check(std::memcmp(wire.payload.data(), prefix, sizeof(prefix)) == 0, "version/kind/session golden scalar bytes");
        const auto restored = Decode(wire);
        Check(restored && restored->input.sourceSequence == 1 && restored->input.captureOrdinal == 20 &&
            restored->input.magnitude == 1 && restored->input.relativeDirection == -1.5f && restored->input.referenceHeading == 2.5f &&
            restored->input.buttons == 1 && restored->input.hostIngressMilliseconds == 0, "native intent preserves exact admitted values");
        auto bad = wire; bad.payload[0] = 4; Check(!Decode(bad), "diagnostic4 cannot be interpreted as diagnostic5");
        bad = wire; bad.payload[0] = 3; Check(!Decode(bad), "V3 cannot be interpreted as diagnostic5");
        bad = wire; bad.classId ^= 1; Check(!Decode(bad), "foreign application class rejected");
        bad = wire; bad.key = static_cast<unsigned>(Kind::Grant); Check(!Decode(bad), "carrier kind must match envelope");
        bad = wire; bad.payload.pop_back(); Check(!Decode(bad), "truncated scalar envelope rejected");
        bad = wire; bad.payload.push_back(0); Check(!Decode(bad), "trailing unqualified bytes rejected");
    }
    void IntentRejectsAndCredits()
    {
        auto m = Input(); m.input.hostIngressMilliseconds = 1001; Check(!Encode(m), "guest cannot spoof host ingress time");
        m = Input(); m.input.buttons = 2; auto ordinary = Decode(Carrier(m));
        Check(ordinary && ordinary->input.buttons == 2, "ordinary attack-held round-trips independently");
        m = Input(); m.input.buttons = 3; auto combined = Decode(Carrier(m));
        Check(combined && combined->input.buttons == 3, "jump and ordinary attack-held remain independent bits");
        m = Input(); m.input.buttons = 4; Check(!Encode(m), "charged/shield/snow/unknown button fields are not admitted");
        m = Input(); m.input.magnitude = std::numeric_limits<float>::quiet_NaN(); Check(!Encode(m), "NaN magnitude rejected");
        m = Input(); m.input.relativeDirection = 4; Check(!Encode(m), "unqualified direction range rejected");
        m = Input(); m.input.sourceSequence = 0; Check(!Encode(m), "sequence zero rejected");
        m = Input(); m.input.grant++; Check(!Encode(m), "record and application grant must match");
        m = Input(); m.input.reserved = 1; Check(!Encode(m), "reserved IPC/wire field must remain zero");
        m = Grant(); Check(Decode(Carrier(m)).has_value(), "bounded initial grant round-trips");
        m.maximumSourceSequence = 9; Check(!Encode(m), "more than8 unacknowledged credits rejected");
        m = Grant(); m.acknowledgedSourceSequence = 8; Check(!Encode(m), "empty/reversed credit window rejected");
        m = Grant(); m.input = Input().input; Check(!Encode(m), "control message cannot carry hidden input");
    }
    void ReadinessAndTerminal()
    {
        auto m = Base(Kind::Hello); m.connectionToken = m.inputEpoch = 0; m.sourceRevision = 1; m.flags = 1;
        Check(Decode(Carrier(m)).has_value(), "explicit source readiness in initial hello");
        m.connectionToken = 1; Check(!Encode(m), "guest Hello cannot claim host token");
        m = Base(Kind::SourceState); m.sourceRevision = 2; m.flags = 0;
        Check(Decode(Carrier(m)).has_value(), "source pause revision carries no input or grant");
        m.sourceRevision = 0; Check(!Encode(m), "source readiness revision cannot be zero");
        m = Base(Kind::Pause); m.flags = NeutralRequired; Check(Decode(Carrier(m)).has_value(), "host pause explicitly requires neutral");
        m = Base(Kind::Stop); m.terminalReason = static_cast<unsigned>(Terminal::Overflow);
        Check(Decode(Carrier(m)).has_value(), "out-of-band terminal reason remains distinct from input");
        m.terminalReason = 0; Check(!Encode(m), "terminal message cannot silently resume");
    }
    void ExistingPacketCarrier()
    {
        using namespace winxcoop::networking;
        SessionConfig hc; hc.role = SessionRole::Host; hc.local = {0x7F000001, 42000};
        SessionConfig gc; gc.role = SessionRole::Guest; gc.local = {0x7F000001, 42001}; gc.server = hc.local;
        SessionCore host(hc), guest(gc);
        Check(host.HostStart(0xE404, 1000) && host.Attach(1000) && guest.Attach(1000), "real SessionCore handshake initialized");
        auto transfer = [](SessionCore& source, SessionCore& target)
        {
            while(auto frame = source.TakeOutbound())
                for(const auto byte : *frame) Check(target.Receive(&byte, 1, 1000) == 1, "common Packet accepts actual fragmented stream");
        };
        for(unsigned pass = 0; pass < 8; ++pass) { transfer(host, guest); transfer(guest, host); }
        Check(host.Phase() == SessionPhase::Running && guest.Phase() == SessionPhase::Running, "native carrier starts only after Ready/host token");
        const auto app = Carrier(Input());
        Check(guest.SendApplicationMessage(app.classId, app.key, app.payload.data(), static_cast<std::uint16_t>(app.payload.size()), 1000),
            "diagnostic uses existing SessionCore application carrier");
        transfer(guest, host); const auto received = host.TakeMessage();
        const auto restored = received ? Decode(*received) : std::nullopt;
        Check(restored && restored->input.captureOrdinal == 20 && !host.TakeMessage(),
            "one fragmented common Packet yields exactly one intact diagnostic intent");
    }
#ifdef _WIN32
    void StatusPublication()
    {
        BridgeStatus slot{}, value{}, copy{}; value.state = static_cast<unsigned>(BridgeState::Ready); value.inputEpoch = 3;
        Check(PublishStatus(slot, value) && ReadStatus(slot, copy) && copy.inputEpoch == 3, "stable publication copied");
        Release(slot.publication, 3); Check(!ReadStatus(slot, copy), "reader never accepts writer-in-progress status");
        Release(slot.publication, 0xFFFFFFFEu); Check(!PublishStatus(slot, value), "publication counter never wraps");
        std::uint32_t terminal = 0; Latch(terminal, Terminal::Overflow); Latch(terminal, Terminal::Transport);
        Check(Acquire(terminal) == static_cast<unsigned>(Terminal::Overflow), "first terminal reason preserved");
    }

    std::uint64_t ProcessCreation(HANDLE process)
    {
        FILETIME creation{}, exit{}, kernel{}, user{};
        Check(GetProcessTimes(process, &creation, &exit, &kernel, &user) != FALSE, "fixture process creation identity available");
        return (std::uint64_t(creation.dwHighDateTime) << 32) | creation.dwLowDateTime;
    }
    std::wstring QuoteArgument(const std::wstring& value)
    {
        std::wstring result = L"\""; unsigned slashes = 0;
        for(const auto c : value)
        {
            if(c == L'\\') { ++slashes; continue; }
            if(c == L'\"') { result.append(slashes * 2 + 1, L'\\'); result += c; }
            else { result.append(slashes, L'\\'); result += c; }
            slashes = 0;
        }
        result.append(slashes * 2, L'\\'); result += L'\"'; return result;
    }
    class OwnedBridgeProcess final
    {
        PROCESS_INFORMATION process_{};
    public:
        OwnedBridgeProcess(const std::filesystem::path& executable, const std::wstring& arguments)
        {
            std::wstring command = QuoteArgument(executable.wstring()) + L" " + arguments;
            STARTUPINFOW startup{}; startup.cb = sizeof(startup);
            Check(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                nullptr, nullptr, &startup, &process_) != FALSE, "real diagnostic bridge process launched hidden");
        }
        ~OwnedBridgeProcess()
        {
            if(process_.hProcess && WaitForSingleObject(process_.hProcess, 0) == WAIT_TIMEOUT)
            {
                // This is the exact helper process created by this fixture.
                // Never enumerate/terminate an unrelated process on failure.
                (void)TerminateProcess(process_.hProcess, 101); (void)WaitForSingleObject(process_.hProcess, 2000);
            }
            if(process_.hThread) CloseHandle(process_.hThread);
            if(process_.hProcess) CloseHandle(process_.hProcess);
        }
        HANDLE Handle() const noexcept { return process_.hProcess; }
        DWORD Pid() const noexcept { return process_.dwProcessId; }
    };
    class OwnedMappingView final
    {
        HANDLE mapping_ = nullptr;
        Shared* view_ = nullptr;
    public:
        ~OwnedMappingView() { if(view_) UnmapViewOfFile(view_); if(mapping_) CloseHandle(mapping_); }
        bool TryOpen(const std::wstring& name)
        {
            if(view_) return true;
            mapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
            if(!mapping_) return false;
            view_ = static_cast<Shared*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
            if(!view_) { CloseHandle(mapping_); mapping_ = nullptr; return false; }
            return true;
        }
        Shared& Get() { Check(view_ != nullptr, "fixture mapping is open"); return *view_; }
    };
    std::uint16_t ReadyPort(const std::filesystem::path& file)
    {
        std::ifstream input(file, std::ios::binary);
        if(!input) return 0;
        const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        const auto start = text.find("\"port\":"); if(start == std::string::npos) return 0;
        unsigned port = 0; const char* p = text.data() + start + 7;
        const auto parsed = std::from_chars(p, text.data() + text.size(), port);
        return parsed.ec == std::errc{} && port <= 65535 ? static_cast<std::uint16_t>(port) : 0;
    }
    void FreshFixtureFile(const std::filesystem::path& path, const std::string& text)
    {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        Check(file != INVALID_HANDLE_VALUE, "fixture evidence file is fresh"); DWORD written = 0;
        const bool ok = text.size() <= MAXDWORD && WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size();
        CloseHandle(file); Check(ok, "fixture evidence written completely");
    }
    void CachedReadyProcessCase(const std::filesystem::path& executable, const std::filesystem::path& directory, bool odd, bool extendedActorLease = false)
    {
        using namespace winxcoop::networking;
        const std::string caseName = extendedActorLease ? "actor-lease-2000-gap" : odd ? "odd-publication" : "zero-pid";
        const std::wstring suffix = (extendedActorLease ? L"Lease2000." : odd ? L"Odd." : L"Pid0.") + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
        const std::wstring mappingName = L"Local\\WinxCoop.DiagnosticInput5.Regression." + suffix;
        const std::uint64_t nonce = (GetTickCount64() << 16) ^ GetCurrentProcessId() ^ (odd ? 1ull : 2ull);
        const auto readyFile = directory / (caseName + ".ready.json"), reportFile = directory / (caseName + ".bridge.json");
        const std::wstring arguments = L"--role host --bind 127.0.0.1:0 --mapping " + QuoteArgument(mappingName) +
            L" --nonce " + std::to_wstring(nonce) + L" --session 601 --scenario 602 --level-epoch 1 --generation 1 --run-ms 10000 --ready-file " +
            QuoteArgument(readyFile.wstring()) + L" --report " + QuoteArgument(reportFile.wstring()) + (extendedActorLease ? L" --actor-lease-ms 2000" : L"");
        OwnedBridgeProcess child(executable, arguments); OwnedMappingView mapped;
        std::uint16_t port = 0; const auto openDeadline = GetTickCount64() + 2500;
        BridgeStatus bridge{};
        while(GetTickCount64() < openDeadline)
        {
            Check(WaitForSingleObject(child.Handle(), 0) == WAIT_TIMEOUT, "bridge remains alive until mapping claim");
            if(mapped.TryOpen(mappingName) && ReadStatus(mapped.Get().bridge, bridge) && bridge.publication != 0 && (port = ReadyPort(readyFile)) != 0) break;
            Sleep(2);
        }
        Check(port != 0, "real bridge created initialized mapping and ephemeral listener"); Shared& ipc = mapped.Get();
        Check(ipc.header.magic == IpcMagic && ipc.header.version == IpcVersion && ipc.header.bytes == sizeof(Shared) &&
            ipc.header.capacity == RingCapacity && ipc.header.nonce == nonce && ipc.header.ownerPid == child.Pid() &&
            ipc.header.ownerCreationFileTime == ProcessCreation(child.Handle()) && ipc.header.role == static_cast<unsigned>(Role::Host),
            "real bridge process/mapping identity is exact");

        ActorStatus actor{}; actor.participantPid = GetCurrentProcessId(); actor.participantCreationFileTime = ProcessCreation(GetCurrentProcess());
        actor.availability = static_cast<unsigned>(Availability::Ready); actor.readinessRevision = 1; actor.observedAtMilliseconds = GetTickCount64();
        Check(PublishStatus(ipc.actor, actor), "fixture claims real mapped actor status");
        TcpTransportConfig config; config.role = SessionRole::Guest; config.localBind = {0x7F000001, 0}; config.server = Endpoint{0x7F000001, port};
        WindowsTcpTransport client(config); Check(client.Start(GetTickCount64()), "fixture connects through existing real TCP/Packet stack");
        bool hello = false, admitted = false; auto lastHeartbeat = actor.observedAtMilliseconds;
        const auto admitDeadline = GetTickCount64() + 1500;
        while(GetTickCount64() < admitDeadline)
        {
            const auto now = GetTickCount64(); client.Pump(now); while(client.TakeMessage()) {}
            if(now - lastHeartbeat >= 20)
            { actor.observedAtMilliseconds = now; Check(PublishStatus(ipc.actor, actor), "actor heartbeat before admission"); lastHeartbeat = now; }
            if(!hello && client.Session() && client.Session()->Phase() == SessionPhase::Running)
            {
                Message message; message.kind = Kind::Hello; message.sessionId = 601; message.scenarioId = 602;
                message.levelEpoch = message.bodyGeneration = 1; message.sourceRevision = 1; message.flags = 1;
                const auto bytes = Encode(message); Check(bytes.has_value(), "source-ready Hello encodes through common stream");
                hello = client.SendApplicationMessage(client.ActiveConnectionId(), ApplicationClass, static_cast<unsigned>(Kind::Hello), bytes->data(), WireBytes, now);
            }
            if(ReadStatus(ipc.bridge, bridge) && bridge.state == static_cast<unsigned>(BridgeState::Ready) && bridge.grant && bridge.inputEpoch)
            { admitted = true; break; }
            Check(WaitForSingleObject(child.Handle(), 0) == WAIT_TIMEOUT, "bridge alive until positive Ready admission"); Sleep(2);
        }
        Check(admitted, "real bridge positively admits mapped live actor Ready before fault injection");
        auto observation = GetTickCount64(); actor.observedAtMilliseconds = observation;
        Check(PublishStatus(ipc.actor, actor), "fixture publishes final fresh Ready observation");
        // Require later bridge loop publication while actor remains readable,
        // so cached Ready is established before making the writer unreadable.
        const auto cacheDeadline = observation + 100;
        while(GetTickCount64() < cacheDeadline)
        {
            client.Pump(GetTickCount64()); while(client.TakeMessage()) {}
            if(ReadStatus(ipc.bridge, bridge) && bridge.bridgeTickMilliseconds >= observation + 15) break;
            Sleep(1);
        }
        Check(bridge.bridgeTickMilliseconds >= observation + 15, "bridge loops admitted final cached Ready observation");
        std::uint64_t survivedHeartbeatGap = 0;
        if(extendedActorLease)
        {
            const auto oldGrant = bridge;
            const auto gapObservation = observation;
            while(GetTickCount64() - gapObservation < 650)
            {
                client.Pump(GetTickCount64()); while(client.TakeMessage()) {}
                Check(WaitForSingleObject(child.Handle(), 0) == WAIT_TIMEOUT, "optional2000ms actor lease survives650ms missing heartbeat");
                if(ReadStatus(ipc.bridge, bridge)) Check(!bridge.terminalReason && bridge.state == static_cast<unsigned>(BridgeState::Ready),
                    "optional2000ms actor lease keeps published control nonterminal during650ms missing heartbeat");
                Sleep(1);
            }
            survivedHeartbeatGap = GetTickCount64() - gapObservation;
            Check(bridge.inputEpoch > oldGrant.inputEpoch && GetTickCount64() > oldGrant.leaseDeadlineMilliseconds &&
                oldGrant.leaseDeadlineMilliseconds <= oldGrant.bridgeTickMilliseconds + LeaseMilliseconds,
                "data authority still expires/rearms at500ms despite the longer actor liveness lease");
            Message stale; stale.kind = Kind::Input; stale.sessionId = 601; stale.scenarioId = 602; stale.connectionToken = oldGrant.connectionToken;
            stale.levelEpoch = stale.bodyGeneration = 1; stale.inputEpoch = oldGrant.inputEpoch; stale.grant = oldGrant.grant;
            stale.leaseDeadlineMilliseconds = oldGrant.leaseDeadlineMilliseconds;
            stale.input = {oldGrant.inputEpoch, 1, 1, gapObservation, 0, 1, 0, 0, 0, oldGrant.grant, 0, oldGrant.leaseDeadlineMilliseconds};
            const auto bytes = Encode(stale); Check(bytes.has_value(), "old data authority fixture uses the original application codec");
            Check(client.SendApplicationMessage(client.ActiveConnectionId(), ApplicationClass, static_cast<unsigned>(Kind::Input), bytes->data(), WireBytes, GetTickCount64()),
                "expired input is actually sent over the established real TCP connection");
            observation = GetTickCount64(); actor.observedAtMilliseconds = observation;
            Check(PublishStatus(ipc.actor, actor), "same live actor PID and revision refresh after650ms heartbeat gap");
        }
        const auto faultAt = GetTickCount64();
        if(!extendedActorLease && odd)
        {
            const auto publication = Acquire(ipc.actor.publication); Check((publication & 1) == 0, "fault starts from stable actor publication");
            Release(ipc.actor.publication, publication + 1); // Stalled writer stays odd; actor process remains alive.
        }
        else if(!extendedActorLease)
        {
            actor.participantPid = 0; Check(PublishStatus(ipc.actor, actor), "fault publishes stable PID0 after actual binding");
        }
        const auto actorLease = extendedActorLease ? 2000u : LeaseMilliseconds;
        bool fenced = false; const auto leaseDeadline = observation + actorLease + 250;
        while(GetTickCount64() <= leaseDeadline)
        {
            const auto now = GetTickCount64(); client.Pump(now); while(client.TakeMessage()) {}
            if(ReadStatus(ipc.bridge, bridge) && bridge.state == static_cast<unsigned>(BridgeState::Closed)) { fenced = true; break; }
            Sleep(1);
        }
        const auto detectedAt = GetTickCount64();
        Check(WaitForSingleObject(GetCurrentProcess(), 0) == WAIT_TIMEOUT, "claimed actor process is still alive during stale-read regression");
        const auto elapsed = bridge.bridgeTickMilliseconds >= observation ? bridge.bridgeTickMilliseconds - observation : 0;
        const std::string evidence = "{\"case\":\"" + caseName + "\",\"bridgePid\":" + std::to_string(child.Pid()) +
            ",\"actorPid\":" + std::to_string(GetCurrentProcessId()) + ",\"admittedReady\":true,\"actorStillAlive\":true,\"observation\":" +
            std::to_string(observation) + ",\"faultAt\":" + std::to_string(faultAt) + ",\"detectedAt\":" + std::to_string(detectedAt) +
            ",\"closed\":" + (fenced ? "true" : "false") + ",\"terminalReason\":" + std::to_string(bridge.terminalReason) +
            ",\"bridgeLeaseAgeMs\":" + std::to_string(elapsed) + ",\"actorLeaseMillis\":" + std::to_string(actorLease) +
            ",\"survivedHeartbeatGapMs\":" + std::to_string(survivedHeartbeatGap) + ",\"schedulerAllowanceMs\":250,\"gameplayQualified\":false}\n";
        FreshFixtureFile(directory / (caseName + ".fixture.json"), evidence); std::cout << evidence;
        Check(fenced && bridge.terminalReason == static_cast<unsigned>(Terminal::LeaseExpired) &&
            Acquire(ipc.input.producerTerminal) == static_cast<unsigned>(Terminal::LeaseExpired),
            "cached Ready lease fences despite unreadable/zero-PID fresh actor status");
        Check(elapsed > actorLease && elapsed <= actorLease + 250, "real process fence honors configured actor lease plus250ms scheduler allowance");
        Check(WaitForSingleObject(child.Handle(), 1000) == WAIT_OBJECT_0, "fenced bridge process exits promptly and releases owned listener");
        DWORD exitCode = 0; Check(GetExitCodeProcess(child.Handle(), &exitCode) && exitCode == 2, "expected terminal fence is visible in child exit status");
        if(extendedActorLease)
        {
            std::ifstream report(reportFile, std::ios::binary); const std::string text((std::istreambuf_iterator<char>(report)), std::istreambuf_iterator<char>());
            Check(Acquire(ipc.input.writeCursor) == 0 && text.find("\"actorLeaseMillis\":2000,") != std::string::npos &&
                text.find("\"staleEpochs\":1,") != std::string::npos && text.find("\"received\":0,") != std::string::npos,
                "longer actor lease never admits expired old-epoch input; actual bridge report pins the separation");
        }
        client.Close();
    }
    std::uint64_t PerformanceTicks()
    { LARGE_INTEGER value{}; Check(QueryPerformanceCounter(&value) != FALSE, "fixture performance clock available"); return static_cast<std::uint64_t>(value.QuadPart); }
    void ContinuousCreditProcessCase(const std::filesystem::path& executable, const std::filesystem::path& directory)
    {
        const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
        const auto hostName = L"Local\\WinxCoop.DiagnosticInput5.CreditHost." + suffix;
        const auto guestName = L"Local\\WinxCoop.DiagnosticInput5.CreditGuest." + suffix;
        const std::uint64_t hostNonce = (GetTickCount64() << 16) ^ GetCurrentProcessId(), guestNonce = hostNonce + 1;
        const auto hostReady = directory / "credit-host.ready.json", guestReady = directory / "credit-guest.ready.json";
        const auto hostReport = directory / "credit-host.bridge.json", guestReport = directory / "credit-guest.bridge.json";
        auto arguments = [&](bool host)
        {
            return std::wstring(L"--role ") + (host ? L"host" : L"guest") + L" --bind 127.0.0.1:0 --mapping " + QuoteArgument(host ? hostName : guestName) +
                L" --nonce " + std::to_wstring(host ? hostNonce : guestNonce) + L" --session 701 --scenario 702 --level-epoch 1 --generation 1 --run-ms 15000 --ready-file " +
                QuoteArgument((host ? hostReady : guestReady).wstring()) + L" --report " + QuoteArgument((host ? hostReport : guestReport).wstring());
        };
        OwnedBridgeProcess host(executable, arguments(true)); OwnedMappingView hostView, guestView;
        std::uint16_t port = 0; const auto hostDeadline = GetTickCount64() + 2500;
        BridgeStatus hs{}, gs{};
        while(GetTickCount64() < hostDeadline)
        {
            Check(WaitForSingleObject(host.Handle(), 0) == WAIT_TIMEOUT, "credit Host process live during setup");
            if(hostView.TryOpen(hostName) && ReadStatus(hostView.Get().bridge, hs) && hs.publication && (port = ReadyPort(hostReady))) break;
            Sleep(2);
        }
        Check(port != 0, "credit Host opened real ephemeral TCP listener");
        OwnedBridgeProcess guest(executable, arguments(false) + L" --server 127.0.0.1:" + std::to_wstring(port));
        const auto guestDeadline = GetTickCount64() + 2500;
        while(GetTickCount64() < guestDeadline)
        {
            Check(WaitForSingleObject(guest.Handle(), 0) == WAIT_TIMEOUT, "credit Guest process live during setup");
            if(guestView.TryOpen(guestName) && ReadStatus(guestView.Get().bridge, gs) && gs.publication) break;
            Sleep(2);
        }
        Check(gs.publication != 0, "credit Guest initialized real mapping");
        Shared& hi = hostView.Get(); Shared& gi = guestView.Get();
        Check(hi.header.ownerPid == host.Pid() && gi.header.ownerPid == guest.Pid() && hi.header.nonce == hostNonce && gi.header.nonce == guestNonce &&
            hi.header.ownerCreationFileTime == ProcessCreation(host.Handle()) && gi.header.ownerCreationFileTime == ProcessCreation(guest.Handle()),
            "credit pair uses two distinct exact bridge process/mapping owners");
        ActorStatus ha{}, ga{};
        ha.participantPid = ga.participantPid = GetCurrentProcessId(); ha.participantCreationFileTime = ga.participantCreationFileTime = ProcessCreation(GetCurrentProcess());
        ha.availability = ga.availability = static_cast<unsigned>(Availability::Ready); ha.readinessRevision = ga.readinessRevision = 1;
        auto heartbeat = [&]
        {
            ha.observedAtMilliseconds = ga.observedAtMilliseconds = GetTickCount64();
            Check(PublishStatus(hi.actor, ha) && PublishStatus(gi.actor, ga), "credit fixture actor statuses published");
        };
        heartbeat(); const auto admitDeadline = GetTickCount64() + 1500;
        while(GetTickCount64() < admitDeadline)
        {
            heartbeat();
            if(ReadStatus(hi.bridge, hs) && ReadStatus(gi.bridge, gs) && hs.state == static_cast<unsigned>(BridgeState::Ready) &&
                gs.state == static_cast<unsigned>(BridgeState::Ready) && hs.inputEpoch && hs.inputEpoch == gs.inputEpoch) break;
            Sleep(2);
        }
        Check(hs.state == static_cast<unsigned>(BridgeState::Ready) && gs.state == static_cast<unsigned>(BridgeState::Ready),
            "two real bridge processes completed Ready/grant handshake");
        const auto epoch = gs.inputEpoch; constexpr std::uint32_t records = 120;
        LARGE_INTEGER frequency{}; Check(QueryPerformanceFrequency(&frequency) && frequency.QuadPart >= 60, "fixture60Hz clock frequency");
        const auto step = static_cast<std::uint64_t>(frequency.QuadPart) / 60;
        auto nextSource = PerformanceTicks() + step, nextAck = nextSource + step / 2;
        struct RawSample { std::uint32_t epoch; std::uint64_t ordinal, captureTime; float magnitude, direction, heading; std::uint32_t buttons; };
        std::deque<RawSample> rawPending; std::array<RawSample, records> original{};
        std::uint64_t firstCapture = 0, lastCapture = 0, largestGap = 0, previousCapture = 0, creditWaits = 0, maximumRawAge = 0;
        std::uint32_t captured = 0, published = 0, delivered = 0, acknowledged = 0, peakOutstanding = 0, maximumRaw = 0, previousGrant = 0, grantChanges = 0;
        std::optional<InputRecord> pendingAck; std::optional<BridgeStatus> observedGrant, initialGrant;
        auto lastHeartbeat = GetTickCount64();
        const auto runDeadline = GetTickCount64() + 5000;
        while(GetTickCount64() < runDeadline)
        {
            if(!ReadStatus(hi.bridge, hs) || !ReadStatus(gi.bridge, gs)) { Sleep(0); continue; }
            Check(!hs.terminalReason && !gs.terminalReason && hs.state == static_cast<unsigned>(BridgeState::Ready) &&
                gs.state == static_cast<unsigned>(BridgeState::Ready) && hs.inputEpoch == epoch && gs.inputEpoch == epoch,
                "continuous credit run never fences, pauses, or re-arms into another epoch");
            if(gs.grant != previousGrant) { previousGrant = gs.grant; ++grantChanges; }
            const auto qpc = PerformanceTicks();
            bool ackChanged = false;
            if(qpc >= nextAck)
            {
                nextAck += step;
                if(pendingAck)
                {
                    acknowledged = pendingAck->sourceSequence; ha.appliedEpoch = epoch; ha.appliedSourceSequence = acknowledged; pendingAck.reset(); ackChanged = true;
                }
                const auto write = Acquire(hi.input.writeCursor), read = Acquire(hi.input.readCursor);
                Check(write >= read && write - read <= RingCapacity, "real Host delivery FIFO bounded");
                if(write != read)
                {
                    const InputRecord record = hi.input.records[read % RingCapacity]; MemoryBarrier();
                    Check(ValidInput(record, false) && record.inputEpoch == epoch && record.sourceSequence == delivered + 1 &&
                        record.captureOrdinal == record.sourceSequence && record.sourceSequence <= captured &&
                        record.hostIngressMilliseconds > 0 && record.hostIngressMilliseconds <= GetTickCount64(),
                        "each60Hz record is delivered in order with original host ingress time");
                    const auto& expected = original[record.sourceSequence - 1];
                    Check(record.inputEpoch == expected.epoch && record.captureOrdinal == expected.ordinal && record.captureMilliseconds == expected.captureTime &&
                        record.magnitude == expected.magnitude && record.relativeDirection == expected.direction && record.referenceHeading == expected.heading &&
                        record.buttons == expected.buttons, "raw captured epoch/ordinal/time/all input values survive FIFO and actual wire unchanged");
                    ++delivered; Release(hi.input.readCursor, read + 1); pendingAck = record;
                }
            }
            if(qpc >= nextSource && captured < records)
            {
                nextSource += step;
                if(observedGrant)
                {
                    Check(observedGrant->inputEpoch == gs.inputEpoch && observedGrant->connectionToken == gs.connectionToken,
                        "same-epoch credit refresh requires an earlier Ready source tick with the same connection token");
                    Check(rawPending.size() < RingCapacity, "bounded raw capture FIFO must never overwrite or coalesce");
                    const auto ordinal = captured + 1;
                    RawSample sample{epoch, ordinal, GetTickCount64(), 1.0f, (ordinal & 1) ? 0.25f : -0.25f,
                        (ordinal & 1) ? -0.5f : 0.5f, (ordinal != 1 && ordinal % 30 == 0 ? 1u : 0u) |
                            ((ordinal >= 12 && ordinal <= 14) || (ordinal >= 28 && ordinal <= 32) ? 2u : 0u)};
                    if(!captured) initialGrant = *observedGrant; // Initial neutral arm retains the prior Ready tick grant.
                    original[captured++] = sample; rawPending.push_back(sample);
                    maximumRaw = (std::max)(maximumRaw, static_cast<std::uint32_t>(rawPending.size()));
                    if(!firstCapture) firstCapture = qpc;
                    if(previousCapture) largestGap = (std::max)(largestGap, qpc - previousCapture);
                    previousCapture = lastCapture = qpc;
                }
                observedGrant = gs; // New epoch/initial arm needs the earlier Ready tick; same-epoch credit refresh does not re-arm.
            }
            while(!rawPending.empty())
            {
                const auto& sample = rawPending.front(); const auto now = GetTickCount64();
                Check(sample.epoch == epoch && now >= sample.captureTime && now - sample.captureTime <= LeaseMilliseconds,
                    "unpublished raw source intent retains its original timestamp and500ms local age bound");
                maximumRawAge = (std::max)(maximumRawAge, now - sample.captureTime);
                const auto grant = published ? gs : *initialGrant;
                if(published + 1 > grant.maximumSourceSequence) { ++creditWaits; break; }
                const auto write = Acquire(gi.input.writeCursor), read = Acquire(gi.input.readCursor);
                Check(write >= read && write - read < RingCapacity, "wire capture ring remains bounded independently of raw FIFO");
                // Allocate wire sequence/grant only at publication. Raw pending
                // samples have no transport identity to relabel; already
                // published IPC records are never changed or overwritten.
                InputRecord record{sample.epoch, published + 1, sample.ordinal, sample.captureTime, 0,
                    sample.magnitude, sample.direction, sample.heading, sample.buttons, grant.grant, 0, grant.leaseDeadlineMilliseconds};
                gi.input.records[write % RingCapacity] = record; MemoryBarrier(); Release(gi.input.writeCursor, write + 1);
                ++published; rawPending.pop_front();
            }
            Check(published >= acknowledged && published - acknowledged <= MaximumCredits, "actual unacknowledged source window remains<=8");
            peakOutstanding = (std::max)(peakOutstanding, published - acknowledged);
            if(ackChanged || GetTickCount64() - lastHeartbeat >= 10) { heartbeat(); lastHeartbeat = GetTickCount64(); }
            if(captured == records && rawPending.empty() && published == records && delivered == records && acknowledged == records && gs.acknowledgedSourceSequence == records) break;
            // QPC scheduling keeps this finite contract near60Hz without a
            // process-wide timer-resolution change or pretending to InputRET.
            Sleep(0);
        }
        const auto duration = (lastCapture - firstCapture) * 1000 / static_cast<std::uint64_t>(frequency.QuadPart);
        const auto maximumGap = largestGap * 1000 / static_cast<std::uint64_t>(frequency.QuadPart);
        const bool complete = captured == records && rawPending.empty() && published == records && delivered == records && acknowledged == records && gs.acknowledgedSourceSequence == records;
        const std::string evidence = "{\"case\":\"continuous-credit\",\"bridgeProcesses\":2,\"captured\":" + std::to_string(captured) + ",\"sourceRecords\":" + std::to_string(published) +
            ",\"delivered\":" + std::to_string(delivered) + ",\"fixtureAppliedAck\":" + std::to_string(acknowledged) +
            ",\"returnedGuestAck\":" + std::to_string(gs.acknowledgedSourceSequence) + ",\"observedGrantChanges\":" + std::to_string(grantChanges) +
            ",\"peakUnacknowledged\":" + std::to_string(peakOutstanding) + ",\"sourceDurationMs\":" + std::to_string(duration) +
            ",\"maximumSourceGapMs\":" + std::to_string(maximumGap) + ",\"singleEpoch\":" + std::to_string(epoch) +
            ",\"maximumRawPending\":" + std::to_string(maximumRaw) + ",\"creditWaitIterations\":" + std::to_string(creditWaits) +
            ",\"maximumRawAgeMs\":" + std::to_string(maximumRawAge) + ",\"sourceEndedAndDrained\":" + (complete ? "true" : "false") +
            ",\"allCapturedPayloadsAndTimesPreserved\":true,\"sourceFasterThanServerDurabilityPromised\":false" +
            ",\"newEpochAndInitialArmRequirePriorTick\":true,\"alreadyArmedSameEpochUsesCurrentCredit\":true,\"actualNativeInputRET\":false,\"actualNativeWorldRET\":false,\"gameplayQualified\":false}\n";
        FreshFixtureFile(directory / "continuous-credit.fixture.json", evidence); std::cout << evidence;
        Check(complete && published > MaximumCredits && grantChanges >= 30, "continuous real TCP/IPC progress spans many old8-credit windows and frequent ACK renewals");
        Check(duration >= 1800 && duration <= 2300 && maximumGap <= 100, "continuous source is near60Hz within explicit scheduler bounds");
        ga.availability = ha.availability = static_cast<unsigned>(Availability::Retired); ++ga.readinessRevision; ++ha.readinessRevision;
        heartbeat(); Check(WaitForSingleObject(guest.Handle(), 1500) == WAIT_OBJECT_0 && WaitForSingleObject(host.Handle(), 1500) == WAIT_OBJECT_0,
            "both finite credit bridge processes stop after fixture retirement");
    }
    void RealBridgeProcessRegressions(const std::filesystem::path& executable, const std::filesystem::path& directory)
    {
        Check(std::filesystem::is_regular_file(executable), "explicit diagnostic bridge executable exists");
        Check(std::filesystem::create_directory(directory), "real process regression evidence directory must be fresh");
        CachedReadyProcessCase(executable, directory, true); CachedReadyProcessCase(executable, directory, false);
        ContinuousCreditProcessCase(executable, directory);
        CachedReadyProcessCase(executable, directory, false, true);
    }
#endif
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char**)
#endif
{
    try
    {
        RoundTripAndCarrierBoundaries(); IntentRejectsAndCredits(); ReadinessAndTerminal(); ExistingPacketCarrier();
#ifdef _WIN32
        StatusPublication();
        if(argc != 1)
        {
            Check(argc == 5 && std::wstring(argv[1]) == L"--bridge-process" && std::wstring(argv[3]) == L"--process-report-dir",
                "optional CLI: --bridge-process EXE --process-report-dir FRESH_DIRECTORY");
            RealBridgeProcessRegressions(std::filesystem::path(argv[2]), std::filesystem::path(argv[4]));
        }
#else
        Check(argc == 1, "process regression is Windows-only");
#endif
        std::cout << "DiagnosticInputProtocolTests PASS (contract only; no native/guest gameplay claim)\n"; return 0;
    }
    catch(const std::exception& e) { std::cerr << "DiagnosticInputProtocolTests FAIL: " << e.what() << '\n'; return 1; }
}
