#define WINXCOOP_BOOTSTRAP_NATIVE_BUILD
#include "BootstrapNative.h"
#include "BootstrapPeer.h"
#include <cstddef>
#include <cstring>
#include <new>
#include <stdexcept>
#include <type_traits>

#if defined(_MSC_VER) && defined(_M_IX86)
#define WCB_EXPORT(name) __pragma(comment(linker, "/EXPORT:" #name "=_" #name))
WCB_EXPORT(wc_bootstrap_abi_version)
WCB_EXPORT(wc_bootstrap_get_abi_layout)
WCB_EXPORT(wc_bootstrap_create)
WCB_EXPORT(wc_bootstrap_destroy)
WCB_EXPORT(wc_bootstrap_start)
WCB_EXPORT(wc_bootstrap_pump)
WCB_EXPORT(wc_bootstrap_publish)
WCB_EXPORT(wc_bootstrap_try_read_bootstrap)
WCB_EXPORT(wc_bootstrap_send_control)
WCB_EXPORT(wc_bootstrap_try_read_control)
WCB_EXPORT(wc_bootstrap_status)
WCB_EXPORT(wc_bootstrap_ready_receipt_bytes)
WCB_EXPORT(wc_bootstrap_ready_state_bytes)
WCB_EXPORT(wc_bootstrap_observe_ready)
WCB_EXPORT(wc_bootstrap_ready_state)
WCB_EXPORT(wc_bootstrap_send_notice)
WCB_EXPORT(wc_bootstrap_try_read_notice)
WCB_EXPORT(wc_bootstrap_stop)
#undef WCB_EXPORT
#endif

namespace b = winxcoop::bootstrap;
static_assert(sizeof(WcbDigest) == 32 && sizeof(WcbCreateArgs) == 80);
static_assert(sizeof(WcbAssignment) == 136 && sizeof(WcbStatus) == 280 && sizeof(WcbAbiLayout) == 48);
static_assert(sizeof(WcbReadyReceipt) == 56 && sizeof(WcbReadyState) == 48);
static_assert(sizeof(WcbOwnerNotice) == 40);
static_assert(offsetof(WcbAssignment, hostInputNonce) == 40 && offsetof(WcbAssignment, levelEpoch) == 72);
static_assert(offsetof(WcbAssignment, payloadDigest) == 104 && offsetof(WcbStatus, assignment) == 144);
static_assert(std::is_standard_layout_v<WcbAssignment> && std::is_trivially_copyable_v<WcbAssignment>);
static_assert(WCB_MAX_BOOTSTRAP_BYTES == b::MaximumBootstrapBytes && WCB_CONTROL_BYTES == b::ControlBytes);

struct WcbPeerHandle final
{
    b::Peer peer;
    explicit WcbPeerHandle(b::Config value) : peer(std::move(value)) {}
};
namespace
{
    b::Digest In(const WcbDigest& value) noexcept
    { b::Digest result{}; std::memcpy(result.data(), &value, sizeof(value)); return result; }
    WcbDigest Out(const b::Digest& value) noexcept
    { WcbDigest result{}; std::memcpy(&result, value.data(), sizeof(result)); return result; }
    b::Assignment In(const WcbAssignment& a) noexcept
    {
        return {a.sessionId, a.scenarioId, a.inputToken, a.worldToken,
            a.hostInputNonce, a.hostWorldNonce, a.guestInputNonce, a.guestWorldNonce,
            a.levelEpoch, a.generation, a.level, a.entry, a.controlPort, a.inputPort,
            a.worldPort, a.checkpointBytes, In(a.payloadDigest)};
    }
    WcbAssignment Out(const b::Assignment& a) noexcept
    {
        return {WCB_ABI_VERSION, sizeof(WcbAssignment), a.sessionId, a.scenarioId, a.inputToken, a.worldToken,
            a.hostInputNonce, a.hostWorldNonce, a.guestInputNonce, a.guestWorldNonce,
            a.levelEpoch, a.generation, a.level, a.entry, a.controlPort, a.inputPort,
            a.worldPort, a.checkpointBytes, Out(a.payloadDigest)};
    }
    int32_t Result(bool value) noexcept { return value ? WCB_OK : WCB_FAILED; }
    template<class Action> int32_t Guard(WcbPeerHandle* handle, Action action) noexcept
    {
        if (!handle) return WCB_INVALID_ARGUMENT;
        try { return action(handle->peer); }
        catch (const std::bad_alloc&) { handle->peer.Stop(b::Terminal::Backpressure); return WCB_FAILED; }
        catch (...) { handle->peer.Stop(b::Terminal::Internal); return WCB_INTERNAL_ERROR; }
    }
    bool ValidArgs(const WcbCreateArgs& a) noexcept
    {
        if (a.reserved || (a.role != WCB_ROLE_HOST && a.role != WCB_ROLE_GUEST) || a.bindPort > UINT16_MAX ||
            a.serverPort > UINT16_MAX || b::Empty(In(a.profile)) || a.connectTimeoutMilliseconds < 100 ||
            a.connectTimeoutMilliseconds > 600000 || a.transferTimeoutMilliseconds < 100 ||
            a.transferTimeoutMilliseconds > 600000 || a.heartbeatMilliseconds < 100 ||
            a.heartbeatMilliseconds > 60000 || a.idleTimeoutMilliseconds <= a.heartbeatMilliseconds ||
            a.idleTimeoutMilliseconds > 600000) return false;
        return a.role == WCB_ROLE_HOST ? (!a.serverIpv4 && !a.serverPort) : (a.serverIpv4 && a.serverPort);
    }
}

uint32_t WCB_CALL wc_bootstrap_abi_version(void) { return WCB_ABI_VERSION; }
uint32_t WCB_CALL wc_bootstrap_ready_receipt_bytes(void) { return sizeof(WcbReadyReceipt); }
uint32_t WCB_CALL wc_bootstrap_ready_state_bytes(void) { return sizeof(WcbReadyState); }
int32_t WCB_CALL wc_bootstrap_get_abi_layout(WcbAbiLayout* output, uint32_t bytes)
{
    if (!output) return WCB_INVALID_ARGUMENT;
    if (bytes != sizeof(*output)) return WCB_ABI_MISMATCH;
    *output = {WCB_ABI_VERSION, sizeof(*output), sizeof(WcbCreateArgs), sizeof(WcbAssignment), sizeof(WcbStatus),
        offsetof(WcbAssignment, hostInputNonce), offsetof(WcbAssignment, levelEpoch), offsetof(WcbAssignment, payloadDigest),
        offsetof(WcbStatus, assignment), WCB_SEED_WITNESS_BYTES, WCB_MAX_CHECKPOINT_BYTES, WCB_MAX_BOOTSTRAP_BYTES};
    return WCB_OK;
}
int32_t WCB_CALL wc_bootstrap_create(const WcbCreateArgs* args, WcbPeerHandle** output)
{
    if (!args || !output) return WCB_INVALID_ARGUMENT;
    if (args->abiVersion != WCB_ABI_VERSION || args->structBytes != sizeof(*args)) return WCB_ABI_MISMATCH;
    if (!ValidArgs(*args)) return WCB_INVALID_ARGUMENT;
    try
    {
        b::Config config; config.role = static_cast<b::Role>(args->role); config.profile = In(args->profile);
        config.transferTimeoutMilliseconds = args->transferTimeoutMilliseconds;
        config.transport.localBind = {args->bindIpv4, static_cast<uint16_t>(args->bindPort)};
        if (args->role == WCB_ROLE_GUEST) config.transport.server = winxcoop::networking::Endpoint{args->serverIpv4, static_cast<uint16_t>(args->serverPort)};
        config.transport.connectTimeoutMilliseconds = args->connectTimeoutMilliseconds;
        config.transport.heartbeatMilliseconds = args->heartbeatMilliseconds;
        config.transport.timeoutMilliseconds = args->idleTimeoutMilliseconds;
        *output = new WcbPeerHandle(std::move(config)); return WCB_OK;
    }
    catch (const std::invalid_argument&) { return WCB_INVALID_ARGUMENT; }
    catch (...) { return WCB_INTERNAL_ERROR; }
}
void WCB_CALL wc_bootstrap_destroy(WcbPeerHandle* handle) { delete handle; }
int32_t WCB_CALL wc_bootstrap_start(WcbPeerHandle* handle, uint64_t now)
{ return Guard(handle, [&](auto& p) { return Result(p.Start(now)); }); }
int32_t WCB_CALL wc_bootstrap_pump(WcbPeerHandle* handle, uint64_t now)
{ return Guard(handle, [&](auto& p) { return Result(p.Pump(now)); }); }
int32_t WCB_CALL wc_bootstrap_publish(WcbPeerHandle* handle, const WcbAssignment* a,
    const uint8_t* payload, uint32_t bytes, uint64_t now)
{
    if (!a || !payload) return WCB_INVALID_ARGUMENT;
    if (a->abiVersion != WCB_ABI_VERSION || a->structBytes != sizeof(*a)) return WCB_ABI_MISMATCH;
    if (!b::ValidPayloadBytes(bytes) || bytes != WCB_SEED_WITNESS_BYTES + a->checkpointBytes) return WCB_INVALID_ARGUMENT;
    return Guard(handle, [&](auto& p) { return Result(p.Publish(In(*a), {payload, payload + bytes}, now)); });
}
int32_t WCB_CALL wc_bootstrap_try_read_bootstrap(WcbPeerHandle* handle, WcbAssignment* a, uint32_t assignmentBytes,
    uint8_t* payload, uint32_t capacity, uint32_t* bytes)
{
    if (!handle || !a || !payload || !bytes) return WCB_INVALID_ARGUMENT;
    if (assignmentBytes != sizeof(*a)) return WCB_ABI_MISMATCH;
    return Guard(handle, [&](auto& p) -> int32_t {
        if (!p.HasBootstrap()) return p.Failure() == b::Terminal::None ? WCB_EMPTY : WCB_FAILED;
        const auto& value = p.BootstrapBytes();
        if (capacity < value.size()) return WCB_BUFFER_TOO_SMALL;
        const auto result = Out(p.CurrentAssignment());
        std::memcpy(payload, value.data(), value.size()); *a = result; *bytes = static_cast<uint32_t>(value.size());
        (void)p.ConsumeBootstrap(); return WCB_OK;
    });
}
int32_t WCB_CALL wc_bootstrap_send_control(WcbPeerHandle* handle, const uint8_t* bytes, uint32_t count, uint64_t now)
{
    if (!bytes || count != WCB_CONTROL_BYTES) return WCB_INVALID_ARGUMENT;
    return Guard(handle, [&](auto& p) -> int32_t {
        switch (p.SendControl(bytes, count, now))
        { case b::Admission::Accepted: return WCB_OK; case b::Admission::Full: return WCB_BACKPRESSURE; default: return WCB_FAILED; }
    });
}
int32_t WCB_CALL wc_bootstrap_try_read_control(WcbPeerHandle* handle, uint8_t* bytes, uint32_t capacity)
{
    if (!handle || !bytes) return WCB_INVALID_ARGUMENT;
    return Guard(handle, [&](auto& p) -> int32_t {
        if (!p.HasControl()) return p.Failure() == b::Terminal::None ? WCB_EMPTY : WCB_FAILED;
        if (capacity < WCB_CONTROL_BYTES) return WCB_BUFFER_TOO_SMALL;
        const auto& value = p.FrontControl(); std::memcpy(bytes, value.data(), value.size()); p.ConsumeControl(); return WCB_OK;
    });
}
int32_t WCB_CALL wc_bootstrap_status(const WcbPeerHandle* handle, WcbStatus* output, uint32_t bytes)
{
    if (!handle || !output) return WCB_INVALID_ARGUMENT;
    if (bytes != sizeof(*output)) return WCB_ABI_MISMATCH;
    const auto s = handle->peer.Snapshot();
    *output = {WCB_ABI_VERSION, sizeof(*output), static_cast<uint32_t>(s.phase), static_cast<uint32_t>(s.terminal),
        static_cast<uint32_t>(s.role), s.profileMatched, s.bootstrapAvailable, s.bootstrapAcknowledged,
        s.bound.address, s.bound.port, s.peer.address, s.peer.port,
        s.connectionId, s.transferId, s.totalBytes, s.receivedBytes, s.controlQueued, s.pendingOutbound, s.ownedSockets, 0,
        s.socketsOpened, s.socketsClosed, s.wsaStarts, s.wsaCleanups, s.lastPumpMilliseconds, s.lastProgressMilliseconds, Out(s.assignment)};
    return WCB_OK;
}
int32_t WCB_CALL wc_bootstrap_observe_ready(WcbPeerHandle* handle, const WcbReadyReceipt* value, uint64_t now)
{
    if (!value || value->reserved) return WCB_INVALID_ARGUMENT;
    if (value->abiVersion != WCB_ABI_VERSION || value->structBytes != sizeof(*value)) return WCB_ABI_MISMATCH;
    const b::ReadyReceipt receipt{static_cast<b::Role>(value->role), value->fullSnapshotApplied,
        value->freshNeutralInput, value->sessionId, value->levelEpoch, value->generation, value->revision, value->nativeTick};
    if (!b::ValidReady(receipt)) return WCB_INVALID_ARGUMENT;
    return Guard(handle, [&](auto& p) { return Result(p.ObserveReady(receipt, now)); });
}
int32_t WCB_CALL wc_bootstrap_ready_state(const WcbPeerHandle* handle, WcbReadyState* output, uint32_t bytes)
{
    if (!handle || !output) return WCB_INVALID_ARGUMENT;
    if (bytes != sizeof(*output)) return WCB_ABI_MISMATCH;
    const auto& peer = handle->peer; const auto& receipt = peer.PeerReadyReceipt();
    const auto& assignment = peer.CurrentAssignment();
    *output = {WCB_ABI_VERSION, sizeof(*output), peer.LocalReady(), peer.RemoteReady(), peer.BothReady(),
        static_cast<uint32_t>(receipt.role), assignment.levelEpoch, assignment.generation, receipt.revision, receipt.nativeTick};
    return WCB_OK;
}
int32_t WCB_CALL wc_bootstrap_send_notice(WcbPeerHandle* handle, const WcbOwnerNotice* value, uint64_t now)
{
    if (!value) return WCB_INVALID_ARGUMENT;
    if (value->abiVersion != WCB_ABI_VERSION || value->structBytes != sizeof(*value)) return WCB_ABI_MISMATCH;
    const b::OwnerNotice notice{static_cast<b::Role>(value->role), static_cast<b::NoticeKind>(value->kind),
        value->sessionId, value->oldEpoch, value->oldGeneration, value->operationId};
    if (!b::ValidNotice(notice)) return WCB_INVALID_ARGUMENT;
    return Guard(handle, [&](auto& p) -> int32_t {
        switch (p.SendNotice(notice, now))
        { case b::Admission::Accepted: return WCB_OK; case b::Admission::Full: return WCB_BACKPRESSURE; default: return WCB_FAILED; }
    });
}
int32_t WCB_CALL wc_bootstrap_try_read_notice(WcbPeerHandle* handle, WcbOwnerNotice* output, uint32_t bytes)
{
    if (!output) return WCB_INVALID_ARGUMENT;
    if (bytes != sizeof(*output)) return WCB_ABI_MISMATCH;
    return Guard(handle, [&](auto& p) -> int32_t {
        if (!p.HasNotice()) return p.Failure() == b::Terminal::None ? WCB_EMPTY : WCB_FAILED;
        const auto& value = p.FrontNotice();
        *output = {WCB_ABI_VERSION, sizeof(*output), static_cast<uint32_t>(value.role), static_cast<uint32_t>(value.kind),
            value.sessionId, value.oldEpoch, value.oldGeneration, value.operationId};
        p.ConsumeNotice(); return WCB_OK;
    });
}
void WCB_CALL wc_bootstrap_stop(WcbPeerHandle* handle, uint32_t terminal)
{ if (handle) handle->peer.Stop(static_cast<b::Terminal>(terminal)); }
