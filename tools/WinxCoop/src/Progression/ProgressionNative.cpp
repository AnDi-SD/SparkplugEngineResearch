#define WINXCOOP_PROGRESSION_NATIVE_BUILD
#include "ProgressionNative.h"
#include "ProgressionSession.h"
#include <cstring>
#include <new>
#include <stdexcept>
#include <type_traits>

// Keep undecorated C names usable by P/Invoke in the x86 build as well.
#if defined(_MSC_VER) && defined(_M_IX86)
#define WCP_EXPORT(name) __pragma(comment(linker, "/EXPORT:" #name "=_" #name))
WCP_EXPORT(wc_progression_abi_version)
WCP_EXPORT(wc_progression_wire_bytes)
WCP_EXPORT(wc_progression_create)
WCP_EXPORT(wc_progression_destroy)
WCP_EXPORT(wc_progression_begin)
WCP_EXPORT(wc_progression_retired)
WCP_EXPORT(wc_progression_host_entry)
WCP_EXPORT(wc_progression_guest_applied)
WCP_EXPORT(wc_progression_receive_wire)
WCP_EXPORT(wc_progression_take_load)
WCP_EXPORT(wc_progression_peek_wire)
WCP_EXPORT(wc_progression_commit)
WCP_EXPORT(wc_progression_poll)
WCP_EXPORT(wc_progression_status)
WCP_EXPORT(wc_progression_stop)
#undef WCP_EXPORT
#endif

namespace p = winxcoop::progression;
static_assert(WCP_WIRE_BYTES == p::WireBytes);
static_assert(sizeof(WcpDigest) == 32 && sizeof(WcpScene) == 8);
static_assert(sizeof(WcpDescriptor) == 40 && sizeof(WcpConnection) == 24);
static_assert(sizeof(WcpChannels) == 56 && sizeof(WcpDecision) == 72);
static_assert(sizeof(WcpCreateArgs) == 152 && sizeof(WcpRetirementReceipt) == 48);
static_assert(sizeof(WcpEntryReceipt) == 184 && sizeof(WcpAppliedReceipt) == 88);
static_assert(sizeof(WcpMessage) == 248 && sizeof(WcpStatus) == 104);
static_assert(std::is_standard_layout_v<WcpMessage> && std::is_trivially_copyable_v<WcpMessage>);

struct WcpSessionHandle final
{
    p::Session session;
    WcpSessionHandle(p::Role role, p::ConnectionIdentity connection,
        p::TrustedInitialRunning initial, uint64_t timeout)
        : session(role, connection, initial, timeout) {}
};

namespace
{
    p::Digest In(const WcpDigest& value) noexcept
    { p::Digest result{}; std::memcpy(result.data(), &value, sizeof(value)); return result; }
    WcpDigest Out(const p::Digest& value) noexcept
    { WcpDigest result{}; std::memcpy(&result, value.data(), sizeof(result)); return result; }
    p::SceneIdentity In(WcpScene value) noexcept { return {value.levelEpoch, value.generation}; }
    WcpScene Out(p::SceneIdentity value) noexcept { return {value.levelEpoch, value.generation}; }
    p::LevelDescriptor In(const WcpDescriptor& value) noexcept { return {value.level, value.entry, In(value.digest)}; }
    WcpDescriptor Out(const p::LevelDescriptor& value) noexcept { return {value.level, value.entry, Out(value.digest)}; }
    p::ConnectionIdentity In(WcpConnection value) noexcept { return {value.sessionId, value.scenarioId, value.token}; }
    WcpConnection Out(p::ConnectionIdentity value) noexcept { return {value.sessionId, value.scenarioId, value.token}; }
    p::ChannelGeneration In(WcpChannels value) noexcept
    { return {value.sessionId, value.inputToken, value.worldToken, value.hostInputNonce,
        value.hostWorldNonce, value.guestInputNonce, value.guestWorldNonce}; }
    WcpChannels Out(p::ChannelGeneration value) noexcept
    { return {value.sessionId, value.inputToken, value.worldToken, value.hostInputNonce,
        value.hostWorldNonce, value.guestInputNonce, value.guestWorldNonce}; }
    p::Decision In(const WcpDecision& value) noexcept
    { return {value.operation, In(value.previous), In(value.next), static_cast<p::Reason>(value.reason), In(value.destination)}; }
    WcpDecision Out(const p::Decision& value) noexcept
    { return {value.operation, Out(value.previous), Out(value.next), static_cast<uint32_t>(value.reason), 0, Out(value.destination)}; }
    p::NativeEntryReceipt In(const WcpEntryReceipt& value) noexcept
    { return {In(value.scene), In(value.descriptor), In(value.channels), In(value.seed), In(value.witness),
        value.localEntryOrdinal, value.originalLoadReturned != 0, value.actualReadyNativeWorld != 0}; }
    WcpMessage Out(const p::Message& value) noexcept
    { return {static_cast<uint32_t>(value.kind), static_cast<uint32_t>(value.terminal), Out(value.connection),
        Out(value.decision), Out(value.channels), Out(value.seed), Out(value.witness),
        value.appliedRevision, value.nativeTick, value.retirementMask, 0}; }
    bool Boolean(uint32_t value) noexcept { return value <= 1; }
    bool EntryFlags(const WcpEntryReceipt& value) noexcept
    { return Boolean(value.originalLoadReturned) && Boolean(value.actualReadyNativeWorld); }
    int32_t Failure(WcpSessionHandle& handle, p::Terminal terminal) noexcept
    { handle.session.Stop(terminal); return WCP_SESSION_FAILED; }
    template<class Action>
    int32_t Guard(WcpSessionHandle* handle, Action action) noexcept
    {
        if (!handle) return WCP_INVALID_ARGUMENT;
        try { return action(*handle); }
        catch (const std::bad_alloc&) { return Failure(*handle, p::Terminal::Backpressure); }
        catch (...) { (void)Failure(*handle, p::Terminal::InvalidRecord); return WCP_INTERNAL_ERROR; }
    }
    int32_t Result(bool accepted) noexcept { return accepted ? WCP_OK : WCP_SESSION_FAILED; }
    std::optional<p::Message> Decode(const uint8_t* data)
    {
        winxcoop::networking::SessionMessage record;
        record.classId = p::ApplicationClass;
        record.payload.assign(data, data + p::WireBytes);
        // The payload itself owns Kind. Let the sole production codec enforce
        // its key/kind equality instead of adding a second header decoder here.
        for (uint32_t key = 1; key <= 6; ++key)
        {
            record.key = key;
            if (auto value = p::Decode(record)) return value;
        }
        return std::nullopt;
    }
}

uint32_t WCP_CALL wc_progression_abi_version(void) { return WCP_ABI_VERSION; }
uint32_t WCP_CALL wc_progression_wire_bytes(void) { return WCP_WIRE_BYTES; }
int32_t WCP_CALL wc_progression_create(const WcpCreateArgs* args, WcpSessionHandle** output)
{
    if (!output) return WCP_INVALID_ARGUMENT;
    *output = nullptr;
    if (!args) return WCP_INVALID_ARGUMENT;
    if (args->abiVersion != WCP_ABI_VERSION || args->structBytes != sizeof(WcpCreateArgs)) return WCP_ABI_MISMATCH;
    if (args->reserved) return WCP_INVALID_ARGUMENT;
    try
    {
        *output = new WcpSessionHandle(static_cast<p::Role>(args->role), In(args->connection),
            {In(args->initialScene), In(args->initialDescriptor), In(args->initialChannels)}, args->transitionTimeoutMilliseconds);
        return WCP_OK;
    }
    catch (const std::invalid_argument&) { return WCP_INVALID_ARGUMENT; }
    catch (...) { return WCP_INTERNAL_ERROR; }
}
void WCP_CALL wc_progression_destroy(WcpSessionHandle* handle) { delete handle; }
int32_t WCP_CALL wc_progression_begin(WcpSessionHandle* handle, const WcpDecision* decision, uint64_t now)
{
    return Guard(handle, [&](auto& h) -> int32_t {
        if (!h.session.Poll(now)) return WCP_SESSION_FAILED;
        if (!decision || decision->reserved) return Failure(h, p::Terminal::InvalidRecord);
        return Result(h.session.Begin(In(*decision), now));
    });
}
int32_t WCP_CALL wc_progression_retired(WcpSessionHandle* handle, const WcpRetirementReceipt* value, uint64_t now)
{
    return Guard(handle, [&](auto& h) -> int32_t {
        if (!h.session.Poll(now)) return WCP_SESSION_FAILED;
        if (!value || value->reserved || !Boolean(value->originalExitReturned) || !Boolean(value->inputRetired) ||
            !Boolean(value->worldRetired) || !Boolean(value->bindingsRetired) || !Boolean(value->borrowedBaselinesRestored) ||
            !Boolean(value->patchesRestored) || !Boolean(value->noNativeOwnership) ||
            !Boolean(value->noPendingNativeCallsOrApply) || !Boolean(value->laterWorldResourcesReleased))
            return Failure(h, p::Terminal::NativeReceipt);
        const p::NativeRetirementReceipt receipt{In(value->scene), value->originalExitReturned != 0,
            value->inputRetired != 0, value->worldRetired != 0, value->bindingsRetired != 0,
            value->borrowedBaselinesRestored != 0, value->patchesRestored != 0, value->noNativeOwnership != 0,
            value->noPendingNativeCallsOrApply != 0, value->laterWorldResourcesReleased != 0};
        return Result(h.session.ObserveLocalRetired(receipt, now));
    });
}
int32_t WCP_CALL wc_progression_host_entry(WcpSessionHandle* handle, const WcpEntryReceipt* value, uint64_t now)
{
    return Guard(handle, [&](auto& h) -> int32_t {
        if (!h.session.Poll(now)) return WCP_SESSION_FAILED;
        if (!value || !EntryFlags(*value)) return Failure(h, p::Terminal::NativeReceipt);
        return Result(h.session.ObserveHostEntry(In(*value), now));
    });
}
int32_t WCP_CALL wc_progression_guest_applied(WcpSessionHandle* handle, const WcpEntryReceipt* entry,
    const WcpAppliedReceipt* value, uint64_t now)
{
    return Guard(handle, [&](auto& h) -> int32_t {
        if (!h.session.Poll(now)) return WCP_SESSION_FAILED;
        if (!entry || !value || !EntryFlags(*entry) || !Boolean(value->completeNativeSnapshotApplied) ||
            !Boolean(value->freshNeutralInputBoundaryObserved)) return Failure(h, p::Terminal::NativeReceipt);
        const p::NativeWorldAppliedReceipt applied{In(value->scene), In(value->channels), value->revision, value->nativeTick,
            value->completeNativeSnapshotApplied != 0, value->freshNeutralInputBoundaryObserved != 0};
        return Result(h.session.ObserveGuestEntryApplied(In(*entry), applied, now));
    });
}
int32_t WCP_CALL wc_progression_receive_wire(WcpSessionHandle* handle, const uint8_t* data, uint32_t bytes,
    uint64_t now, WcpMessage* accepted)
{
    if (accepted) *accepted = {};
    return Guard(handle, [&](auto& h) -> int32_t {
        if (!h.session.Poll(now)) return WCP_SESSION_FAILED;
        if (!data || bytes != p::WireBytes || !accepted) return Failure(h, p::Terminal::InvalidRecord);
        const auto message = Decode(data);
        if (!message) return Failure(h, p::Terminal::InvalidRecord);
        if (!h.session.Receive(*message, now)) return WCP_SESSION_FAILED;
        *accepted = Out(*message); return WCP_OK;
    });
}
int32_t WCP_CALL wc_progression_take_load(WcpSessionHandle* handle, uint64_t now, WcpMessage* output)
{
    if (output) *output = {};
    return Guard(handle, [&](auto& h) -> int32_t {
        if (!h.session.Poll(now)) return WCP_SESSION_FAILED;
        if (!output) return Failure(h, p::Terminal::InvalidRecord);
        const auto message = h.session.TakeLoadAuthorization(now);
        if (!message) return h.session.Failure() == p::Terminal::None ? WCP_EMPTY : WCP_SESSION_FAILED;
        *output = Out(*message); return WCP_OK;
    });
}
int32_t WCP_CALL wc_progression_peek_wire(WcpSessionHandle* handle, uint64_t now, uint8_t* data,
    uint32_t capacity, WcpMessage* output)
{
    if (output) *output = {};
    return Guard(handle, [&](auto& h) -> int32_t {
        if (!h.session.Poll(now)) return WCP_SESSION_FAILED;
        if (!data || capacity < p::WireBytes || !output) return Failure(h, p::Terminal::InvalidRecord);
        const auto message = h.session.PeekOutbound();
        if (!message) return WCP_EMPTY;
        const auto wire = p::Encode(*message);
        if (!wire || wire->size() != p::WireBytes) return Failure(h, p::Terminal::InvalidRecord);
        std::memcpy(data, wire->data(), p::WireBytes); *output = Out(*message); return WCP_OK;
    });
}
int32_t WCP_CALL wc_progression_commit(WcpSessionHandle* handle, uint64_t now)
{ return Guard(handle, [&](auto& h) { return Result(h.session.CommitOutbound(now)); }); }
int32_t WCP_CALL wc_progression_poll(WcpSessionHandle* handle, uint64_t now)
{ return Guard(handle, [&](auto& h) { return Result(h.session.Poll(now)); }); }
int32_t WCP_CALL wc_progression_status(const WcpSessionHandle* handle, WcpStatus* output)
{
    if (!handle || !output) return WCP_INVALID_ARGUMENT;
    const auto& s = handle->session;
    *output = {WCP_ABI_VERSION, sizeof(WcpStatus), static_cast<uint32_t>(s.State()), static_cast<uint32_t>(s.Failure()),
        s.InputAllowed() ? 1u : 0u, static_cast<uint32_t>(s.OutboundCount()), Out(s.CurrentScene()),
        Out(s.CurrentChannels()), s.CompletedOperation(), s.Deadline()};
    return WCP_OK;
}
void WCP_CALL wc_progression_stop(WcpSessionHandle* handle, uint32_t terminal)
{ if (handle) handle->session.Stop(static_cast<p::Terminal>(terminal)); }
