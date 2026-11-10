#pragma once
// In-process C ABI for the production progression Session and its sole wire
// codec. Native receipts remain trusted caller attestations, not game proof.
// Each handle has one serial owner. Destroy only after all calls have returned.
#include <stdint.h>

#if defined(_WIN32)
# if defined(WINXCOOP_PROGRESSION_NATIVE_BUILD)
#  define WCP_API __declspec(dllexport)
# else
#  define WCP_API __declspec(dllimport)
# endif
# define WCP_CALL __cdecl
#else
# define WCP_API
# define WCP_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum { WCP_ABI_VERSION = 1, WCP_WIRE_BYTES = 244 };
enum { WCP_EMPTY = 0, WCP_OK = 1, WCP_INVALID_ARGUMENT = -1,
       WCP_SESSION_FAILED = -2, WCP_ABI_MISMATCH = -3, WCP_INTERNAL_ERROR = -4 };

#pragma pack(push, 8)
// The words are opaque storage for 32 digest bytes, not a numeric hash/wire format.
typedef struct WcpDigest { uint64_t word0, word1, word2, word3; } WcpDigest;
typedef struct WcpScene { uint32_t levelEpoch, generation; } WcpScene;
typedef struct WcpDescriptor { uint32_t level, entry; WcpDigest digest; } WcpDescriptor;
typedef struct WcpConnection { uint64_t sessionId, scenarioId, token; } WcpConnection;
typedef struct WcpChannels {
    uint64_t sessionId, inputToken, worldToken;
    uint64_t hostInputNonce, hostWorldNonce, guestInputNonce, guestWorldNonce;
} WcpChannels;
typedef struct WcpDecision {
    uint64_t operation;
    WcpScene previous, next;
    uint32_t reason, reserved;
    WcpDescriptor destination;
} WcpDecision;
typedef struct WcpCreateArgs {
    uint32_t abiVersion, structBytes, role, reserved;
    WcpConnection connection;
    WcpScene initialScene;
    WcpDescriptor initialDescriptor;
    WcpChannels initialChannels;
    uint64_t transitionTimeoutMilliseconds;
} WcpCreateArgs;
typedef struct WcpRetirementReceipt {
    WcpScene scene;
    uint32_t originalExitReturned, inputRetired, worldRetired, bindingsRetired;
    uint32_t borrowedBaselinesRestored, patchesRestored, noNativeOwnership;
    uint32_t noPendingNativeCallsOrApply, laterWorldResourcesReleased, reserved;
} WcpRetirementReceipt;
typedef struct WcpEntryReceipt {
    WcpScene scene;
    WcpDescriptor descriptor;
    WcpChannels channels;
    WcpDigest seed, witness;
    uint64_t localEntryOrdinal;
    uint32_t originalLoadReturned, actualReadyNativeWorld;
} WcpEntryReceipt;
typedef struct WcpAppliedReceipt {
    WcpScene scene;
    WcpChannels channels;
    uint64_t revision, nativeTick;
    uint32_t completeNativeSnapshotApplied, freshNeutralInputBoundaryObserved;
} WcpAppliedReceipt;
typedef struct WcpMessage {
    uint32_t kind, terminal;
    WcpConnection connection;
    WcpDecision decision;
    WcpChannels channels;
    WcpDigest seed, witness;
    uint64_t appliedRevision, nativeTick;
    uint32_t retirementMask, reserved;
} WcpMessage;
typedef struct WcpStatus {
    uint32_t abiVersion, structBytes, phase, terminal;
    uint32_t inputAllowed, outboundCount;
    WcpScene currentScene;
    WcpChannels currentChannels;
    uint64_t completedOperation, deadlineMilliseconds;
} WcpStatus;
#pragma pack(pop)

typedef struct WcpSessionHandle WcpSessionHandle;
WCP_API uint32_t WCP_CALL wc_progression_abi_version(void);
WCP_API uint32_t WCP_CALL wc_progression_wire_bytes(void);
// Initial Running is an explicit trusted bootstrap, never a native-game receipt.
WCP_API int32_t WCP_CALL wc_progression_create(const WcpCreateArgs*, WcpSessionHandle**);
WCP_API void WCP_CALL wc_progression_destroy(WcpSessionHandle*);
WCP_API int32_t WCP_CALL wc_progression_begin(WcpSessionHandle*, const WcpDecision*, uint64_t now);
WCP_API int32_t WCP_CALL wc_progression_retired(WcpSessionHandle*, const WcpRetirementReceipt*, uint64_t now);
WCP_API int32_t WCP_CALL wc_progression_host_entry(WcpSessionHandle*, const WcpEntryReceipt*, uint64_t now);
WCP_API int32_t WCP_CALL wc_progression_guest_applied(WcpSessionHandle*, const WcpEntryReceipt*, const WcpAppliedReceipt*, uint64_t now);
// Exactly one native-produced 244-byte payload per TCP record. No C# wire codec.
// These APIs do not perform socket I/O. The owner retains a peeked record until
// its entire TCP write succeeds, then commits it once. EOF/error must call Stop.
WCP_API int32_t WCP_CALL wc_progression_receive_wire(WcpSessionHandle*, const uint8_t*, uint32_t bytes, uint64_t now, WcpMessage* accepted);
WCP_API int32_t WCP_CALL wc_progression_take_load(WcpSessionHandle*, uint64_t now, WcpMessage*);
WCP_API int32_t WCP_CALL wc_progression_peek_wire(WcpSessionHandle*, uint64_t now, uint8_t*, uint32_t capacity, WcpMessage*);
WCP_API int32_t WCP_CALL wc_progression_commit(WcpSessionHandle*, uint64_t now);
WCP_API int32_t WCP_CALL wc_progression_poll(WcpSessionHandle*, uint64_t now);
WCP_API int32_t WCP_CALL wc_progression_status(const WcpSessionHandle*, WcpStatus*);
WCP_API void WCP_CALL wc_progression_stop(WcpSessionHandle*, uint32_t terminal);

#ifdef __cplusplus
}
#endif
