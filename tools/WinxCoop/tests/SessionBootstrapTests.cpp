// Native ABI/byte-transfer tests. Loopback sockets and explicit ready fixtures
// do not qualify the game, a second computer, or LAN rendering/input latency.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "SessionRuntime/BootstrapNative.h"
#include "SessionRuntime/BootstrapPeer.h"
#include "Progression/ProgressionProtocol.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace b = winxcoop::bootstrap;
namespace n = winxcoop::networking;
namespace p = winxcoop::progression;
namespace
{
    void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    void Ok(int32_t value, const char* message) { Require(value == WCB_OK, message); }
    WcbDigest Profile() { return {11, 22, 33, 44}; }
    WcbCreateArgs Args(uint32_t role, uint32_t port = 0)
    {
        return {WCB_ABI_VERSION, sizeof(WcbCreateArgs), role, 0x7f000001u, 0,
            role == WCB_ROLE_GUEST ? 0x7f000001u : 0, port, 0, Profile(), 3000, 3000, 100, 10000};
    }
    struct Owner final
    {
        WcbPeerHandle* handle{};
        explicit Owner(const WcbCreateArgs& args) { Ok(wc_bootstrap_create(&args, &handle), "create ABI peer"); }
        ~Owner() { wc_bootstrap_destroy(handle); }
        Owner(const Owner&) = delete; Owner& operator=(const Owner&) = delete;
        WcbStatus Status() const
        { WcbStatus value{}; Ok(wc_bootstrap_status(handle, &value, sizeof(value)), "status ABI"); return value; }
        WcbReadyState Ready() const
        { WcbReadyState value{}; Ok(wc_bootstrap_ready_state(handle, &value, sizeof(value)), "ready state ABI"); return value; }
        void Stop()
        {
            wc_bootstrap_stop(handle, WCB_TERMINAL_LOCAL_STOP);
            const auto status = Status();
            Require(status.ownedSockets == 0 && status.socketsOpened == status.socketsClosed &&
                status.wsaStarts == status.wsaCleanups, "stop balances socket and WSA ownership");
        }
    };
    struct Pair final
    {
        Owner host{Args(WCB_ROLE_HOST)};
        Owner guest;
        static WcbCreateArgs StartHost(Owner& host)
        {
            Ok(wc_bootstrap_start(host.handle, GetTickCount64()), "start host");
            const auto status = host.Status(); Require(status.boundPort != 0, "real OS listener port");
            return Args(WCB_ROLE_GUEST, status.boundPort);
        }
        Pair() : guest(StartHost(host))
        {
            Ok(wc_bootstrap_start(guest.handle, GetTickCount64()), "start guest");
            Until([&] { return host.Status().profileMatched && guest.Status().profileMatched; });
        }
        template<class Predicate> void Until(Predicate predicate, uint64_t limit = 4000)
        {
            const auto deadline = GetTickCount64() + limit;
            do {
                const auto now = GetTickCount64();
                Ok(wc_bootstrap_pump(host.handle, now), "pump host");
                Ok(wc_bootstrap_pump(guest.handle, now), "pump guest");
                if (predicate()) return;
                Sleep(1);
            } while (GetTickCount64() < deadline);
            throw std::runtime_error("bounded loopback predicate deadline");
        }
    };
    WcbAssignment Assignment(uint32_t port, uint32_t checkpointBytes = 0, uint32_t generation = 1)
    {
        const auto input = port == 19001 ? 19003u : 19001u;
        const auto world = port == 19002 ? 19004u : 19002u;
        const uint64_t base = 1000ull * generation;
        return {WCB_ABI_VERSION, sizeof(WcbAssignment), base + 1, 77, base + 2, base + 3,
            base + 4, base + 5, base + 6, base + 7, generation, generation, 28, 0,
            port, input, world, checkpointBytes, {}};
    }
    std::vector<uint8_t> Payload(uint32_t count)
    { std::vector<uint8_t> result(count); for (size_t i = 0; i < result.size(); ++i) result[i] = static_cast<uint8_t>(i * 17 + i / 239); return result; }
    WcbReadyReceipt Ready(const WcbAssignment& a, uint32_t role, uint64_t revision)
    { return {WCB_ABI_VERSION, sizeof(WcbReadyReceipt), role, role == WCB_ROLE_GUEST ? 1u : 0u,
        a.sessionId, a.levelEpoch, a.generation, 1, 0, revision, revision + 10}; }
    std::vector<uint8_t> Control()
    {
        p::Message message; message.connection = {501, 502, 503};
        message.decision = {1, {1, 1}, {2, 2}, p::Reason::Reload, {28, 0, {1}}};
        const auto result = p::Encode(message); Require(result && result->size() == WCB_CONTROL_BYTES, "existing Progression codec fixture"); return *result;
    }
    void AbiAndProtocol()
    {
        Require(wc_bootstrap_abi_version() == 1 && wc_bootstrap_ready_receipt_bytes() == 56 &&
            wc_bootstrap_ready_state_bytes() == 48, "stable readiness ABI size exports");
        WcbAbiLayout layout{}; Ok(wc_bootstrap_get_abi_layout(&layout, sizeof(layout)), "ABI layout");
        Require(layout.createArgsBytes == 80 && layout.assignmentBytes == 136 && layout.statusBytes == 280 &&
            layout.assignmentNonceOffset == 40 && layout.assignmentEpochOffset == 72 && layout.assignmentDigestOffset == 104 &&
            layout.statusAssignmentOffset == 144 && layout.maximumBootstrapBytes == 66048, "actual ABI layout and bounds");
        auto preserved = layout; Require(wc_bootstrap_get_abi_layout(&layout, 1) == WCB_ABI_MISMATCH &&
            std::memcmp(&layout, &preserved, sizeof(layout)) == 0, "ABI failure leaves output unchanged");
        WcbPeerHandle* unchanged = reinterpret_cast<WcbPeerHandle*>(static_cast<uintptr_t>(1));
        auto args = Args(WCB_ROLE_HOST); args.abiVersion = 99;
        Require(wc_bootstrap_create(&args, &unchanged) == WCB_ABI_MISMATCH && unchanged == reinterpret_cast<WcbPeerHandle*>(static_cast<uintptr_t>(1)), "create failure output unchanged");
        args = Args(WCB_ROLE_HOST); args.bindPort = 65536;
        Require(wc_bootstrap_create(&args, &unchanged) == WCB_INVALID_ARGUMENT, "reject truncated port");
        args = Args(WCB_ROLE_GUEST); Require(wc_bootstrap_create(&args, &unchanged) == WCB_INVALID_ARGUMENT, "Guest requires server");
        const std::array<uint8_t, 32> abc{{0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
            0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad}};
        Require(b::ComputeDigest("abc", 3) == abc, "OS SHA256 known answer");
        b::Message chunk; chunk.kind = b::Kind::Chunk; chunk.transferId = 1; chunk.totalBytes = 512;
        chunk.chunk.assign(b::MaximumChunkBytes, 17);
        auto encoded = b::Encode(chunk); Require(encoded && encoded->size() == 256, "bounded full Packet payload");
        n::SessionMessage record{b::ApplicationClass, static_cast<uint32_t>(chunk.kind), *encoded};
        Require(b::Decode(record).has_value(), "existing MemoryStream roundtrip");
        ++record.key; Require(!b::Decode(record), "reject key/kind mismatch"); --record.key;
        record.payload.push_back(0); Require(!b::Decode(record), "reject trailing bytes"); record.payload.pop_back();
        record.payload.pop_back(); Require(!b::Decode(record), "reject truncated chunk");
        chunk.offset = 500; Require(!b::Encode(chunk), "reject range overflow");
        chunk.offset = 0; chunk.chunk.push_back(1); Require(!b::Encode(chunk), "reject oversize chunk");
        b::Message ready; ready.kind = b::Kind::OwnerReady;
        ready.ready = {b::Role::Guest, 1, 1, 1, 1, 1, 2, 3};
        encoded = b::Encode(ready); Require(encoded && encoded->size() == 52, "OwnerReady fixed wire");
        ready.ready.fullSnapshotApplied = 0; Require(!b::Encode(ready), "Guest readiness requires full apply assertion");
        b::Message notice; notice.kind = b::Kind::OwnerNotice;
        notice.notice = {b::Role::Host, b::NoticeKind::HostInputFenced, 1, 1, 1, 1};
        encoded = b::Encode(notice); Require(encoded && encoded->size() == 40, "owner notice fixed wire");
        notice.notice.role = b::Role::Guest; Require(!b::Encode(notice), "notice authority shape rejects Guest fencing Host");
    }
    void TransferReadyControlAndGeneration()
    {
        Pair pair; const auto bytes = Payload(WCB_MAX_BOOTSTRAP_BYTES);
        auto assignment = Assignment(pair.host.Status().boundPort, WCB_MAX_CHECKPOINT_BYTES);
        auto ready = Ready(assignment, WCB_ROLE_HOST, 10);
        Require(wc_bootstrap_observe_ready(pair.host.handle, &ready, GetTickCount64()) == WCB_FAILED, "no readiness before actual byte ACK");
        Ok(wc_bootstrap_publish(pair.host.handle, &assignment, bytes.data(), static_cast<uint32_t>(bytes.size()), GetTickCount64()), "publish max checkpoint");
        pair.Until([&] { return pair.host.Status().bootstrapAcknowledged && pair.guest.Status().bootstrapAvailable; });
        Require(pair.host.Status().phase == WCB_PHASE_BOOTSTRAP_READY && !pair.host.Ready().bothReady &&
            !pair.guest.Ready().bothReady, "byte delivery never creates native owner readiness");
        std::vector<uint8_t> received(bytes.size(), 0xee); WcbAssignment output{}; output.sessionId = 999; uint32_t count = 999;
        Require(wc_bootstrap_try_read_bootstrap(pair.guest.handle, &output, sizeof(output), received.data(), 511, &count) == WCB_BUFFER_TOO_SMALL &&
            count == 999 && output.sessionId == 999 && received[0] == 0xee, "capacity failure neither consumes nor mutates outputs");
        Ok(wc_bootstrap_try_read_bootstrap(pair.guest.handle, &output, sizeof(output), received.data(), static_cast<uint32_t>(received.size()), &count), "read full bootstrap");
        Require(received == bytes && count == bytes.size() && output.checkpointBytes == 65536 && output.hostInputNonce == assignment.hostInputNonce &&
            output.payloadDigest.word0 != 0, "real transfer bytes/digest/identity mapping");
        Require(wc_bootstrap_try_read_bootstrap(pair.guest.handle, &output, sizeof(output), received.data(), static_cast<uint32_t>(received.size()), &count) == WCB_EMPTY, "bootstrap delivered once");
        Ok(wc_bootstrap_publish(pair.host.handle, &assignment, bytes.data(), static_cast<uint32_t>(bytes.size()), GetTickCount64()), "same generation immutable repeat");
        Require(!pair.guest.Status().bootstrapAvailable, "publish repeat never redelivers");
        const auto wire = Control();
        for (unsigned i = 0; i < 4; ++i) Ok(wc_bootstrap_send_control(pair.host.handle, wire.data(), static_cast<uint32_t>(wire.size()), GetTickCount64()), "four bounded outbound copies");
        Require(wc_bootstrap_send_control(pair.host.handle, wire.data(), static_cast<uint32_t>(wire.size()), GetTickCount64()) == WCB_BACKPRESSURE, "full queue accepts no fifth copy");
        pair.Until([&] { return pair.guest.Status().controlQueued == 4; });
        std::array<uint8_t, WCB_CONTROL_BYTES> copy{}; copy.fill(0xdd);
        Require(wc_bootstrap_try_read_control(pair.guest.handle, copy.data(), 243) == WCB_BUFFER_TOO_SMALL && copy[0] == 0xdd, "small control output remains unchanged");
        for (unsigned i = 0; i < 4; ++i) { Ok(wc_bootstrap_try_read_control(pair.guest.handle, copy.data(), static_cast<uint32_t>(copy.size())), "receive existing 244 record"); Require(std::equal(copy.begin(), copy.end(), wire.begin()), "control wire exact"); }
        Require(wc_bootstrap_try_read_control(pair.guest.handle, copy.data(), static_cast<uint32_t>(copy.size())) == WCB_EMPTY, "control consumed once");
        auto guestReady = Ready(assignment, WCB_ROLE_GUEST, 20);
        Ok(wc_bootstrap_observe_ready(pair.guest.handle, &guestReady, GetTickCount64()), "Guest actual apply fixture assertion");
        Ok(wc_bootstrap_observe_ready(pair.host.handle, &ready, GetTickCount64()), "Host actual apply fixture assertion");
        Ok(wc_bootstrap_observe_ready(pair.host.handle, &ready, GetTickCount64()), "exact local readiness repeat");
        pair.Until([&] { return pair.host.Ready().bothReady && pair.guest.Ready().bothReady; });
        Require(pair.host.Ready().peerRevision == 20 && pair.guest.Ready().peerRevision == 10 && pair.host.Status().phase == WCB_PHASE_RUNNING, "opposite owner readiness preserved");
        auto notice = WcbOwnerNotice{WCB_ABI_VERSION, sizeof(WcbOwnerNotice), WCB_ROLE_HOST,
            WCB_NOTICE_HOST_INPUT_FENCED, assignment.sessionId, 1, 1, 1};
        Ok(wc_bootstrap_send_notice(pair.host.handle, &notice, GetTickCount64()), "actual Host input fenced fixture");
        Ok(wc_bootstrap_send_notice(pair.host.handle, &notice, GetTickCount64()), "exact owner notice repeat accepts no second copy");
        WcbOwnerNotice receivedNotice{};
        pair.Until([&] { return wc_bootstrap_try_read_notice(pair.guest.handle, &receivedNotice, sizeof(receivedNotice)) == WCB_OK; });
        Require(receivedNotice.kind == WCB_NOTICE_HOST_INPUT_FENCED && receivedNotice.operationId == 1 &&
            receivedNotice.sessionId == assignment.sessionId, "owner notice complete identity mapping");
        Require(wc_bootstrap_try_read_notice(pair.guest.handle, &receivedNotice, sizeof(receivedNotice)) == WCB_EMPTY, "repeat not delivered twice");
        notice.role = WCB_ROLE_GUEST; notice.kind = WCB_NOTICE_GUEST_PREPARED;
        Ok(wc_bootstrap_send_notice(pair.guest.handle, &notice, GetTickCount64()), "actual Guest Waiting fixture");
        pair.Until([&] { return wc_bootstrap_try_read_notice(pair.host.handle, &receivedNotice, sizeof(receivedNotice)) == WCB_OK; });
        notice.role = WCB_ROLE_HOST; notice.kind = WCB_NOTICE_HOST_REQUEST_ISSUED;
        Ok(wc_bootstrap_send_notice(pair.host.handle, &notice, GetTickCount64()), "actual Host request issued fixture");
        pair.Until([&] { return wc_bootstrap_try_read_notice(pair.guest.handle, &receivedNotice, sizeof(receivedNotice)) == WCB_OK; });
        const auto nextBytes = Payload(512); const auto next = Assignment(pair.host.Status().boundPort, 0, 2);
        Ok(wc_bootstrap_publish(pair.host.handle, &next, nextBytes.data(), static_cast<uint32_t>(nextBytes.size()), GetTickCount64()), "next immutable epoch with fresh tokens/nonces");
        Require(!pair.host.Ready().bothReady && !pair.host.Ready().localReady, "new assignment resets native readiness");
        pair.Until([&] { return pair.host.Status().bootstrapAcknowledged && pair.guest.Status().bootstrapAvailable; });
        Require(!pair.guest.Ready().localReady && !pair.guest.Ready().peerReady && pair.guest.Status().assignment.levelEpoch == 2, "Guest resets old receipts");
        auto changed = nextBytes; changed[0] ^= 1;
        Require(wc_bootstrap_publish(pair.host.handle, &next, changed.data(), static_cast<uint32_t>(changed.size()), GetTickCount64()) == WCB_FAILED &&
            pair.host.Status().terminal == WCB_TERMINAL_SEQUENCE, "changed same-generation payload closes owner");
        pair.host.Stop(); pair.guest.Stop();
    }
    void ProfileClockAndReadinessDeadline()
    {
        Owner host{Args(WCB_ROLE_HOST)}; Ok(wc_bootstrap_start(host.handle, GetTickCount64()), "negative host start");
        auto args = Args(WCB_ROLE_GUEST, host.Status().boundPort); ++args.profile.word0;
        Owner guest{args}; Ok(wc_bootstrap_start(guest.handle, GetTickCount64()), "negative Guest start");
        const auto deadline = GetTickCount64() + 3000;
        while (!host.Status().terminal && !guest.Status().terminal && GetTickCount64() < deadline)
        { (void)wc_bootstrap_pump(host.handle, GetTickCount64()); (void)wc_bootstrap_pump(guest.handle, GetTickCount64()); Sleep(1); }
        Require(host.Status().terminal == WCB_TERMINAL_PROFILE || guest.Status().terminal == WCB_TERMINAL_PROFILE, "profile mismatch rejects before bootstrap");
        Require(!host.Status().bootstrapAvailable && !guest.Status().bootstrapAvailable, "mismatched profile delivers no data"); host.Stop(); guest.Stop();
        Owner clock{Args(WCB_ROLE_HOST)}; Ok(wc_bootstrap_start(clock.handle, 500), "clock host start");
        Require(wc_bootstrap_pump(clock.handle, 499) == WCB_FAILED && clock.Status().terminal == WCB_TERMINAL_CLOCK, "backward clock fails closed"); clock.Stop();
        Pair pair; const auto bytes = Payload(512); const auto a = Assignment(pair.host.Status().boundPort);
        Ok(wc_bootstrap_publish(pair.host.handle, &a, bytes.data(), static_cast<uint32_t>(bytes.size()), GetTickCount64()), "timeout publish");
        pair.Until([&] { return pair.host.Status().bootstrapAcknowledged && pair.guest.Status().bootstrapAvailable; });
        const auto original = pair.host.Status().lastPumpMilliseconds;
        auto ready = Ready(a, WCB_ROLE_HOST, 10); Ok(wc_bootstrap_observe_ready(pair.host.handle, &ready, original + 2900), "local ready before fixed deadline");
        Ok(wc_bootstrap_observe_ready(pair.host.handle, &ready, original + 2999), "repeat does not extend ready deadline");
        Require(wc_bootstrap_pump(pair.host.handle, original + 3000) == WCB_FAILED && pair.host.Status().terminal == WCB_TERMINAL_TIMEOUT, "missing opposite native ready times out despite repeats");
        pair.host.Stop(); pair.guest.Stop();
    }
    void RejectCorruptTransfer(bool badOffset)
    {
        n::TcpTransportConfig transportConfig; transportConfig.localBind = {0x7f000001u, 0};
        transportConfig.heartbeatMilliseconds = 100; transportConfig.timeoutMilliseconds = 10000;
        n::WindowsTcpTransport rawHost(transportConfig);
        Require(rawHost.Start(GetTickCount64()) && rawHost.HostStart(0xE701, GetTickCount64()), "raw Host fixture listener");
        b::Config config; config.role = b::Role::Guest; config.profile[0] = 17;
        config.transport.localBind = {0x7f000001u, 0}; config.transport.server = rawHost.BoundEndpoint();
        config.transport.maxSendChunk = 31; config.transport.maxReceiveChunk = 37;
        config.transport.heartbeatMilliseconds = 100; config.transport.timeoutMilliseconds = 10000;
        b::Peer guest(config); Require(guest.Start(GetTickCount64()), "fragmented Guest start");
        const auto deadline = GetTickCount64() + 4000;
        auto wait = [&](auto predicate) {
            do {
                rawHost.Pump(GetTickCount64()); Require(guest.Pump(GetTickCount64()), "fragmented Guest pump");
                while (rawHost.TakeSessionEvent()) {}
                while (rawHost.TakeMessage()) {}
                if (predicate()) return;
                Sleep(1);
            } while (GetTickCount64() < deadline);
            throw std::runtime_error("raw fixture bounded deadline");
        };
        wait([&] { return rawHost.Session() && rawHost.Session()->Phase() == n::SessionPhase::Running; });
        auto send = [&](b::Message message) {
            const auto wire = b::Encode(message); Require(wire.has_value(), "raw fixture shared codec");
            Require(rawHost.SendApplicationMessage(rawHost.ActiveConnectionId(), b::ApplicationClass,
                static_cast<uint32_t>(message.kind), wire->data(), static_cast<uint16_t>(wire->size()), GetTickCount64()), "raw fixture send full frame");
        };
        b::Message hello; hello.profile = config.profile; send(hello);
        wait([&] { return guest.Snapshot().profileMatched; });
        const auto bytes = Payload(512); b::Assignment assignment;
        assignment.sessionId = 1; assignment.scenarioId = 2; assignment.inputToken = 3; assignment.worldToken = 4;
        assignment.hostInputNonce = 5; assignment.hostWorldNonce = 6; assignment.guestInputNonce = 7; assignment.guestWorldNonce = 8;
        assignment.levelEpoch = assignment.generation = 1; assignment.level = 28;
        assignment.controlPort = rawHost.BoundEndpoint().port;
        assignment.inputPort = assignment.controlPort == 19001 ? 19003 : 19001;
        assignment.worldPort = assignment.controlPort == 19002 ? 19004 : 19002;
        assignment.payloadDigest = b::ComputeDigest(bytes.data(), bytes.size());
        b::Message offer; offer.kind = b::Kind::Offer; offer.transferId = 1; offer.totalBytes = 512; offer.assignment = assignment; send(offer);
        b::Message chunk; chunk.kind = b::Kind::Chunk; chunk.transferId = 1; chunk.totalBytes = 512;
        chunk.offset = badOffset ? 1u : 0u; chunk.chunk.assign(bytes.begin(), bytes.begin() + b::MaximumChunkBytes);
        if (!badOffset) chunk.chunk[0] ^= 1;
        send(chunk);
        if (!badOffset)
        {
            chunk.offset = b::MaximumChunkBytes; chunk.chunk.assign(bytes.begin() + chunk.offset, bytes.begin() + chunk.offset + b::MaximumChunkBytes); send(chunk);
            chunk.offset += b::MaximumChunkBytes; chunk.chunk.assign(bytes.begin() + chunk.offset, bytes.end()); send(chunk);
            b::Message commit; commit.kind = b::Kind::Commit; commit.transferId = 1; commit.totalBytes = 512; commit.digest = assignment.payloadDigest; send(commit);
        }
        do { rawHost.Pump(GetTickCount64()); (void)guest.Pump(GetTickCount64()); if (guest.Failure() != b::Terminal::None) break; Sleep(1); }
        while (GetTickCount64() < deadline);
        Require(guest.Failure() == (badOffset ? b::Terminal::Sequence : b::Terminal::Digest) && !guest.HasBootstrap(), "bad offset/digest cannot deliver bootstrap or create readiness");
        guest.Stop(); rawHost.Close(); const auto status = guest.Snapshot();
        Require(status.ownedSockets == 0 && status.socketsOpened == status.socketsClosed && status.wsaStarts == status.wsaCleanups,
            "rejected transfer balances ownership");
    }
}
int main()
{
    try { AbiAndProtocol(); TransferReadyControlAndGeneration(); ProfileClockAndReadinessDeadline(); RejectCorruptTransfer(false); RejectCorruptTransfer(true);
        std::cout << "WinxCoop bootstrap ABI/codec/loopback ownership contracts PASS\n"; return 0; }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
