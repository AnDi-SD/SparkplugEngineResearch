// C ABI/codec contract tests. Every native receipt in this file is an explicit
// fixture; no game process is started and no gameplay qualification is claimed.
#include "Progression/ProgressionNative.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void Require(bool condition, const char* why)
    { if (!condition) throw std::runtime_error(why); }
    void Ok(int32_t result, const char* why) { Require(result == WCP_OK, why); }
    WcpDigest Digest(uint64_t value) { return {value, value + 1, value + 2, value + 3}; }
    WcpDescriptor Descriptor(uint32_t level = 28) { return {level, 0, Digest(400 + level)}; }
    WcpChannels Channels(uint64_t base)
    { return {base + 1, base + 2, base + 3, base + 4, base + 5, base + 6, base + 7}; }
    bool Same(WcpScene a, WcpScene b)
    { return a.levelEpoch == b.levelEpoch && a.generation == b.generation; }
    bool Same(WcpChannels a, WcpChannels b)
    {
        return a.sessionId == b.sessionId && a.inputToken == b.inputToken && a.worldToken == b.worldToken &&
            a.hostInputNonce == b.hostInputNonce && a.hostWorldNonce == b.hostWorldNonce &&
            a.guestInputNonce == b.guestInputNonce && a.guestWorldNonce == b.guestWorldNonce;
    }
    WcpCreateArgs CreateArgs(uint32_t role, uint64_t timeout = 1000)
    { return {WCP_ABI_VERSION, sizeof(WcpCreateArgs), role, 0, {100, 101, 102}, {1, 1}, Descriptor(), Channels(1000), timeout}; }
    struct Owner final
    {
        WcpSessionHandle* handle{};
        explicit Owner(uint32_t role, uint64_t timeout = 1000)
        { const auto args = CreateArgs(role, timeout); Ok(wc_progression_create(&args, &handle), "create"); Require(handle != nullptr, "create handle"); }
        Owner(const Owner&) = delete;
        Owner& operator=(const Owner&) = delete;
        ~Owner() { wc_progression_destroy(handle); }
        WcpStatus Status() const
        {
            WcpStatus status{}; Ok(wc_progression_status(handle, &status), "status");
            Require(status.abiVersion == WCP_ABI_VERSION && status.structBytes == sizeof(status), "status ABI");
            return status;
        }
    };
    WcpDecision Decision(uint64_t operation = 1, uint32_t old = 1)
    { return {operation, {old, old}, {old + 1, old + 1}, 2, 0, Descriptor()}; }
    WcpRetirementReceipt Retired(WcpScene scene)
    { return {scene, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}; }
    WcpEntryReceipt Entry(const WcpDecision& decision, WcpChannels channels, bool host)
    {
        return {decision.next, decision.destination, channels, Digest(2000 + decision.operation),
            Digest(3000 + decision.operation), host ? 2ull : 1ull, 1, 1};
    }
    WcpAppliedReceipt Applied(const WcpEntryReceipt& entry)
    { return {entry.scene, entry.channels, 42, 17, 1, 1}; }
    using Wire = std::array<uint8_t, WCP_WIRE_BYTES>;
    Wire Peek(Owner& owner, uint64_t now, uint32_t expected)
    {
        Wire wire{}; WcpMessage view{};
        Ok(wc_progression_peek_wire(owner.handle, now, wire.data(), static_cast<uint32_t>(wire.size()), &view), "peek");
        Require(view.kind == expected && view.reserved == 0, "peek kind/canonical POD"); return wire;
    }
    WcpMessage Deliver(Owner& from, Owner& to, uint64_t now, uint32_t kind)
    {
        const auto wire = Peek(from, now, kind);
        Ok(wc_progression_commit(from.handle, now), "commit full fixture transport write");
        WcpMessage accepted{};
        Ok(wc_progression_receive_wire(to.handle, wire.data(), static_cast<uint32_t>(wire.size()), now, &accepted), "receive");
        Require(accepted.kind == kind && accepted.reserved == 0, "accepted native view"); return accepted;
    }
    void BeginPair(Owner& host, Owner& guest, const WcpDecision& decision, uint64_t now)
    {
        Ok(wc_progression_begin(host.handle, &decision, now), "Host begin");
        Require(host.Status().inputAllowed == 0, "Host input barrier at Begin");
        (void)Deliver(host, guest, now, 1);
        Require(guest.Status().inputAllowed == 0, "Guest input barrier at Prepare");
    }
    void RetirePair(Owner& host, Owner& guest, const WcpDecision& decision, uint64_t now)
    {
        const auto receipt = Retired(decision.previous);
        Ok(wc_progression_retired(host.handle, &receipt, now), "Host retirement");
        Ok(wc_progression_retired(guest.handle, &receipt, now), "Guest retirement");
        (void)Deliver(guest, host, now, 2);
    }
    void LoadPair(Owner& host, Owner& guest, const WcpDecision& decision, WcpChannels channels, uint64_t now)
    {
        auto hostEntry = Entry(decision, channels, true);
        if (decision.operation == 2) hostEntry.localEntryOrdinal = 1; // fresh Host process, authoritative payload stays identical
        Ok(wc_progression_host_entry(host.handle, &hostEntry, now), "actual Host fixture entry");
        (void)Deliver(host, guest, now, 3);
        WcpMessage authorization{};
        Ok(wc_progression_take_load(guest.handle, now, &authorization), "Guest exactly-once native load permit");
        Require(authorization.kind == 3 && authorization.decision.operation == decision.operation && Same(authorization.channels, channels), "load authorization identity");
        Require(wc_progression_take_load(guest.handle, now, &authorization) == WCP_EMPTY, "duplicate TakeLoad does not load again");
        const auto guestEntry = Entry(decision, channels, false);
        const auto applied = Applied(guestEntry);
        Ok(wc_progression_guest_applied(guest.handle, &guestEntry, &applied, now), "actual Guest fixture apply");
        (void)Deliver(guest, host, now, 4);
        Require(host.Status().phase == 6 && host.Status().inputAllowed == 0, "Host ResumePending input blocked");
        Require(guest.Status().inputAllowed == 0, "Guest blocked before Resume");
        (void)Deliver(host, guest, now, 5);
        Require(host.Status().inputAllowed == 1 && guest.Status().inputAllowed == 1, "Resume commit/receive admitted");
        Require(Same(host.Status().currentScene, decision.next) && Same(guest.Status().currentScene, decision.next), "new scene committed");
        Require(Same(host.Status().currentChannels, channels) && Same(guest.Status().currentChannels, channels), "fresh channels committed");
    }
    void Replay()
    {
        Owner host(1), guest(2);
        auto decision = Decision(); BeginPair(host, guest, decision, 100);
        const auto oldPrepare = [&] {
            // Produce an independent exact Prepare in a third session.
            Owner sender(1); Ok(wc_progression_begin(sender.handle, &decision, 100), "stale source begin");
            return Peek(sender, 100, 1);
        }();
        RetirePair(host, guest, decision, 101); LoadPair(host, guest, decision, Channels(2000), 102);
        Require(host.Status().completedOperation == 1 && guest.Status().completedOperation == 1, "first operation complete");
        decision = Decision(2, 2); BeginPair(host, guest, decision, 200);
        WcpMessage stale{};
        Ok(wc_progression_receive_wire(guest.handle, oldPrepare.data(), static_cast<uint32_t>(oldPrepare.size()), 201, &stale), "old completed Prepare ignored");
        Require(guest.Status().inputAllowed == 0 && guest.Status().completedOperation == 1, "stale record cannot resume/load");
        RetirePair(host, guest, decision, 202); LoadPair(host, guest, decision, Channels(3000), 203);
        Require(host.Status().completedOperation == 2 && guest.Status().completedOperation == 2, "second operation complete");
    }
    void Corruption()
    {
        Owner source(1); const auto decision = Decision();
        Ok(wc_progression_begin(source.handle, &decision, 10), "corruption source begin");
        const auto original = Peek(source, 10, 1);
        for (unsigned test = 0; test < 4; ++test)
        {
            Owner guest(2); auto wire = original;
            uint32_t length = static_cast<uint32_t>(wire.size());
            if (test == 0) wire[0] ^= 0x40; // protocol version
            if (test == 1) for (unsigned i = 68; i < 100; ++i) wire[i] = 0; // descriptor digest
            if (test == 2) wire[156] = 1; // Prepare must not carry a seed
            if (test == 3) --length;
            WcpMessage accepted{}; accepted.kind = 99;
            Require(wc_progression_receive_wire(guest.handle, wire.data(), length, 10, &accepted) == WCP_SESSION_FAILED, "corrupt independent wire rejected");
            Require(guest.Status().terminal == 1 && guest.Status().inputAllowed == 0 && accepted.kind == 0, "corruption fail-closed/empty output");
        }
        Owner receiverHost(1); WcpMessage accepted{};
        Require(wc_progression_receive_wire(receiverHost.handle, original.data(), static_cast<uint32_t>(original.size()), 10, &accepted) == WCP_SESSION_FAILED, "Host rejects peer Prepare");
        Require(receiverHost.Status().terminal == 3, "Host authority failure");
    }
    void IncompleteRetirement()
    {
        const std::array<uint32_t WcpRetirementReceipt::*, 9> fields = {
            &WcpRetirementReceipt::originalExitReturned, &WcpRetirementReceipt::inputRetired,
            &WcpRetirementReceipt::worldRetired, &WcpRetirementReceipt::bindingsRetired,
            &WcpRetirementReceipt::borrowedBaselinesRestored, &WcpRetirementReceipt::patchesRestored,
            &WcpRetirementReceipt::noNativeOwnership, &WcpRetirementReceipt::noPendingNativeCallsOrApply,
            &WcpRetirementReceipt::laterWorldResourcesReleased};
        for (auto field : fields)
        {
            Owner host(1); const auto decision = Decision(); Ok(wc_progression_begin(host.handle, &decision, 1), "retire begin");
            auto receipt = Retired(decision.previous); receipt.*field = 0;
            Require(wc_progression_retired(host.handle, &receipt, 2) == WCP_SESSION_FAILED, "missing cleanup flag rejected");
            Require(host.Status().terminal == 5 && host.Status().inputAllowed == 0, "incomplete cleanup native failure");
        }
        Owner host(1); const auto decision = Decision(); Ok(wc_progression_begin(host.handle, &decision, 1), "strict bool begin");
        auto receipt = Retired(decision.previous); receipt.originalExitReturned = 2;
        Require(wc_progression_retired(host.handle, &receipt, 2) == WCP_SESSION_FAILED && host.Status().terminal == 5, "ABI bool accepts only zero/one");
    }
    void TimeAndBackpressure()
    {
        Owner host(1, 100); const auto decision = Decision();
        Ok(wc_progression_begin(host.handle, &decision, 10), "timeout begin");
        for (unsigned i = 0; i < 1000; ++i)
        {
            Ok(wc_progression_begin(host.handle, &decision, 11), "exact duplicate Begin");
            (void)Peek(host, 11, 1);
        }
        Require(host.Status().outboundCount == 1 && host.Status().deadlineMilliseconds == 110, "duplicate/peek bounded queue and fixed deadline");
        Wire wire{}; WcpMessage view{};
        Require(wc_progression_peek_wire(host.handle, 110, wire.data(), static_cast<uint32_t>(wire.size()), &view) == WCP_SESSION_FAILED, "Peek checks deadline without explicit Poll");
        Require(host.Status().terminal == 6 && host.Status().outboundCount == 0, "timeout clears pending commands");
        Owner rollback(1); Ok(wc_progression_poll(rollback.handle, 100), "clock initial");
        Require(wc_progression_poll(rollback.handle, 99) == WCP_SESSION_FAILED && rollback.Status().terminal == 7, "clock rollback closes");
        Owner disconnected(2); wc_progression_stop(disconnected.handle, 10);
        Require(disconnected.Status().terminal == 10 && disconnected.Status().inputAllowed == 0, "disconnect terminal");
    }
    void NativeAppliedAndOrdering()
    {
        for (unsigned test = 0; test < 4; ++test)
        {
            Owner host(1), guest(2); const auto decision = Decision();
            BeginPair(host, guest, decision, 1); RetirePair(host, guest, decision, 2);
            const auto channels = Channels(2000); const auto hostEntry = Entry(decision, channels, true);
            Ok(wc_progression_host_entry(host.handle, &hostEntry, 3), "mismatch Host entry");
            (void)Deliver(host, guest, 3, 3);
            WcpMessage authorization{};
            if (test != 0) Ok(wc_progression_take_load(guest.handle, 3, &authorization), "mismatch TakeLoad");
            auto entry = Entry(decision, channels, false); auto applied = Applied(entry);
            if (test == 1) entry.witness.word0 ^= 1;
            if (test == 2) applied.freshNeutralInputBoundaryObserved = 0;
            if (test == 3) applied.revision = 0;
            Require(wc_progression_guest_applied(guest.handle, &entry, &applied, 4) == WCP_SESSION_FAILED, "incomplete/reordered native applied rejected");
            Require(guest.Status().terminal == 5 && guest.Status().inputAllowed == 0, "no native ACK no Resume");
        }
        Owner host(1), guest(2); const auto decision = Decision(); BeginPair(host, guest, decision, 1);
        const auto retirement = Retired(decision.previous);
        Ok(wc_progression_retired(host.handle, &retirement, 2), "Host alone retired");
        const auto entry = Entry(decision, Channels(2000), true);
        Ok(wc_progression_host_entry(host.handle, &entry, 3), "Host entry before Guest cleanup");
        Wire wire{}; WcpMessage view{};
        Require(wc_progression_peek_wire(host.handle, 3, wire.data(), static_cast<uint32_t>(wire.size()), &view) == WCP_EMPTY, "missing peer cleanup withholds Load");
        Require(host.Status().inputAllowed == 0, "no peer cleanup input remains fenced");
    }
    void AbiArguments()
    {
        Require(wc_progression_abi_version() == 1 && wc_progression_wire_bytes() == 244, "version exports");
        auto args = CreateArgs(1); args.abiVersion = 99; WcpSessionHandle* handle{};
        Require(wc_progression_create(&args, &handle) == WCP_ABI_MISMATCH && handle == nullptr, "bad ABI create");
        args = CreateArgs(1); args.structBytes -= 1;
        Require(wc_progression_create(&args, &handle) == WCP_ABI_MISMATCH && handle == nullptr, "bad POD size");
        args = CreateArgs(9);
        Require(wc_progression_create(&args, &handle) == WCP_INVALID_ARGUMENT && handle == nullptr, "bad role create");
        Require(wc_progression_poll(nullptr, 1) == WCP_INVALID_ARGUMENT, "null handle");
        wc_progression_destroy(nullptr);
        Owner host(1); auto decision = Decision(); decision.reserved = 1;
        Require(wc_progression_begin(host.handle, &decision, 1) == WCP_SESSION_FAILED && host.Status().terminal == 1, "reserved fields fail closed");
    }
}
int main()
{
    try
    {
        AbiArguments(); Replay(); Corruption(); IncompleteRetirement(); TimeAndBackpressure(); NativeAppliedAndOrdering();
        std::cout << "ProgressionNative C ABI tests PASS (native receipts are fixtures; gameStarted=false)\n";
        return 0;
    }
    catch (const std::exception& error)
    { std::cerr << "ProgressionNative C ABI tests FAIL: " << error.what() << '\n'; return 1; }
}
