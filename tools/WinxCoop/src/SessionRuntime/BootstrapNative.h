#pragma once
// Local C ABI for one real role-specific bootstrap/control TCP owner. It does
// not create a game or attest native loading, retirement or world application.
// All methods, including destroy, belong to one serial owner; no calls overlap.
#include <stdint.h>

#if defined(_WIN32)
# if defined(WINXCOOP_BOOTSTRAP_NATIVE_BUILD)
#  define WCB_API __declspec(dllexport)
# else
#  define WCB_API __declspec(dllimport)
# endif
# define WCB_CALL __cdecl
#else
# define WCB_API
# define WCB_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum { WCB_ABI_VERSION = 1, WCB_SEED_WITNESS_BYTES = 512,
       WCB_MAX_CHECKPOINT_BYTES = 65536, WCB_MAX_BOOTSTRAP_BYTES = 66048,
       WCB_CONTROL_BYTES = 244 };
enum { WCB_EMPTY = 0, WCB_OK = 1, WCB_INVALID_ARGUMENT = -1,
       WCB_FAILED = -2, WCB_ABI_MISMATCH = -3, WCB_INTERNAL_ERROR = -4,
       WCB_BUFFER_TOO_SMALL = -5, WCB_BACKPRESSURE = -6 };
enum { WCB_ROLE_HOST = 1, WCB_ROLE_GUEST = 2 };
enum { WCB_PHASE_IDLE = 0, WCB_PHASE_LISTENING = 1, WCB_PHASE_CONNECTING = 2,
       WCB_PHASE_HANDSHAKE = 3, WCB_PHASE_PROFILE_MATCHED = 4,
       WCB_PHASE_TRANSFERRING = 5, WCB_PHASE_BOOTSTRAP_READY = 6,
       WCB_PHASE_RUNNING = 7, WCB_PHASE_CLOSED = 8, WCB_PHASE_FAILED = 9 };
enum { WCB_TERMINAL_NONE = 0, WCB_TERMINAL_LOCAL_STOP = 1,
       WCB_TERMINAL_CLOCK = 2, WCB_TERMINAL_TRANSPORT = 3,
       WCB_TERMINAL_TIMEOUT = 4, WCB_TERMINAL_PROFILE = 5,
       WCB_TERMINAL_PROTOCOL = 6, WCB_TERMINAL_SEQUENCE = 7,
       WCB_TERMINAL_BACKPRESSURE = 8, WCB_TERMINAL_DIGEST = 9,
       WCB_TERMINAL_ARGUMENT = 10, WCB_TERMINAL_INTERNAL = 11 };

#pragma pack(push, 8)
typedef struct WcbDigest { uint64_t word0, word1, word2, word3; } WcbDigest;
typedef struct WcbCreateArgs {
    uint32_t abiVersion, structBytes, role, bindIpv4;
    uint32_t bindPort, serverIpv4, serverPort, reserved;
    WcbDigest profile;
    uint32_t connectTimeoutMilliseconds, transferTimeoutMilliseconds;
    uint32_t heartbeatMilliseconds, idleTimeoutMilliseconds;
} WcbCreateArgs;
// IPv4 values use explicit host order: (A<<24)|(B<<16)|(C<<8)|D. Ports must
// fit uint16. Host has no server; Guest must supply one. Profile is nonzero.
// inputToken is also the separate World carrier's uint16 start token;
// worldToken is that carrier's uint64 connection token.
typedef struct WcbAssignment {
    uint32_t abiVersion, structBytes;
    uint64_t sessionId, scenarioId, inputToken, worldToken;
    uint64_t hostInputNonce, hostWorldNonce, guestInputNonce, guestWorldNonce;
    uint32_t levelEpoch, generation, level, entry;
    uint32_t controlPort, inputPort, worldPort, checkpointBytes;
    WcbDigest payloadDigest;
} WcbAssignment;
typedef struct WcbStatus {
    uint32_t abiVersion, structBytes, phase, terminal;
    uint32_t role, profileMatched, bootstrapAvailable, bootstrapAcknowledged;
    uint32_t boundIpv4, boundPort, peerIpv4, peerPort;
    uint64_t connectionId, transferId, totalBytes, receivedBytes;
    uint32_t controlQueued, pendingOutbound, ownedSockets, reserved;
    uint64_t socketsOpened, socketsClosed, wsaStarts, wsaCleanups;
    uint64_t lastPumpMilliseconds, lastProgressMilliseconds;
    WcbAssignment assignment;
} WcbStatus;
// pendingOutbound counts local records, carrier frames and a partial-send
// indicator. Zero means locally drained, never that the remote owner accepted.
// bootstrapAcknowledged is a Host-received byte ACK, never native Loaded.
// Guest uses bootstrapAvailable and its successful once-only read instead.
typedef struct WcbAbiLayout {
    uint32_t abiVersion, structBytes, createArgsBytes, assignmentBytes;
    uint32_t statusBytes, assignmentNonceOffset, assignmentEpochOffset, assignmentDigestOffset;
    uint32_t statusAssignmentOffset, seedWitnessBytes, maximumCheckpointBytes, maximumBootstrapBytes;
} WcbAbiLayout;
typedef struct WcbReadyReceipt {
    uint32_t abiVersion, structBytes, role, fullSnapshotApplied;
    uint64_t sessionId;
    uint32_t levelEpoch, generation, freshNeutralInput, reserved;
    uint64_t revision, nativeTick;
} WcbReadyReceipt;
typedef struct WcbReadyState {
    uint32_t abiVersion, structBytes, localReady, peerReady;
    uint32_t bothReady, peerRole, levelEpoch, generation;
    uint64_t peerRevision, peerNativeTick;
} WcbReadyState;
enum { WCB_NOTICE_GUEST_PREPARED = 1, WCB_NOTICE_HOST_REQUEST_ISSUED = 2,
       WCB_NOTICE_HOST_INPUT_FENCED = 3, WCB_NOTICE_HOST_SESSION_ENDED = 4,
       WCB_NOTICE_GUEST_LEAVE_REQUESTED = 5 };
typedef struct WcbOwnerNotice {
    uint32_t abiVersion, structBytes, role, kind;
    uint64_t sessionId;
    uint32_t oldEpoch, oldGeneration;
    uint64_t operationId;
} WcbOwnerNotice;
#pragma pack(pop)

typedef struct WcbPeerHandle WcbPeerHandle;
WCB_API uint32_t WCB_CALL wc_bootstrap_abi_version(void);
WCB_API int32_t WCB_CALL wc_bootstrap_get_abi_layout(WcbAbiLayout*, uint32_t outputBytes);
WCB_API int32_t WCB_CALL wc_bootstrap_create(const WcbCreateArgs*, WcbPeerHandle**);
WCB_API void WCB_CALL wc_bootstrap_destroy(WcbPeerHandle*);
WCB_API int32_t WCB_CALL wc_bootstrap_start(WcbPeerHandle*, uint64_t nowMilliseconds);
WCB_API int32_t WCB_CALL wc_bootstrap_pump(WcbPeerHandle*, uint64_t nowMilliseconds);
// Only Host may publish actual owner-supplied capture bytes. Payload is exactly
// seed448+witness64 followed by checkpointBytes (0 or349..65536). A zero input
// digest asks native SHA256 to fill the stored/status assignment; nonzero must
// match. Same generation+bytes is idempotent, never a second Guest delivery.
// Next epoch/generation requires previous byte ACK and fresh channel identity;
// this is not permission to bypass the separate native Progression barrier.
WCB_API int32_t WCB_CALL wc_bootstrap_publish(WcbPeerHandle*, const WcbAssignment*,
    const uint8_t* payload, uint32_t payloadBytes, uint64_t nowMilliseconds);
// Guest receives once, after exact offset/total/SHA validation. No pointers,
// native readiness or Loaded receipt are transferred. Failure/empty leaves all
// outputs unchanged. Read Status.totalBytes first to size the output buffer.
WCB_API int32_t WCB_CALL wc_bootstrap_try_read_bootstrap(WcbPeerHandle*,
    WcbAssignment*, uint32_t assignmentBytes, uint8_t* payload,
    uint32_t payloadCapacity, uint32_t* payloadBytes);
// Existing ProgressionProtocol244 records share the same TCP carrier, using its
// existing class/key/codec. Accepted send means this carrier owns a full copy;
// the caller may then Commit its native Progression outbound exactly once.
// Backpressure accepts nothing. Receive pops one complete244-byte record.
WCB_API int32_t WCB_CALL wc_bootstrap_send_control(WcbPeerHandle*,
    const uint8_t* record, uint32_t bytes, uint64_t nowMilliseconds);
WCB_API int32_t WCB_CALL wc_bootstrap_try_read_control(WcbPeerHandle*,
    uint8_t* record, uint32_t capacity);
WCB_API int32_t WCB_CALL wc_bootstrap_status(const WcbPeerHandle*,
    WcbStatus*, uint32_t outputBytes);
WCB_API uint32_t WCB_CALL wc_bootstrap_ready_receipt_bytes(void);
WCB_API uint32_t WCB_CALL wc_bootstrap_ready_state_bytes(void);
// Trusted owner assertion only, AFTER the actual native full apply/neutral
// boundary. This protocol validates role/identity/shape/order, never reads the
// game and never substitutes its validated-byte ACK for either Ready receipt.
WCB_API int32_t WCB_CALL wc_bootstrap_observe_ready(WcbPeerHandle*,
    const WcbReadyReceipt*, uint64_t nowMilliseconds);
WCB_API int32_t WCB_CALL wc_bootstrap_ready_state(const WcbPeerHandle*,
    WcbReadyState*, uint32_t outputBytes);
// Actual intermediate native-owner observations, not P244/native Loaded.
// HostInputFenced permits Guest to stop held-neutral capture and enter Waiting;
// GuestPrepared permits Host to request a transition; HostRequestIssued permits
// Guest retirement. The trusted owner emits only after the corresponding event.
// HostSessionEnded follows both actual retirements; GuestLeaveRequested asks
// the Host owner for that orderly lifecycle. Neither record simulates gameplay.
WCB_API int32_t WCB_CALL wc_bootstrap_send_notice(WcbPeerHandle*,
    const WcbOwnerNotice*, uint64_t nowMilliseconds);
WCB_API int32_t WCB_CALL wc_bootstrap_try_read_notice(WcbPeerHandle*,
    WcbOwnerNotice*, uint32_t outputBytes);
WCB_API void WCB_CALL wc_bootstrap_stop(WcbPeerHandle*, uint32_t terminal);

#ifdef __cplusplus
}
#endif
