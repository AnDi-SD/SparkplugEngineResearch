#pragma once
// Serial-owner local ABI over the existing LobbyDiscovery implementation.
// E7 uses a distinct UDP domain; actual compatibility is checked by bootstrap
// TCP before any game state is transferred. No game/process access belongs here.
#include <stdint.h>
#if defined(_WIN32)
# if defined(WINXCOOP_DISCOVERY_NATIVE_BUILD)
#  define WCD_API __declspec(dllexport)
# else
#  define WCD_API __declspec(dllimport)
# endif
# define WCD_CALL __cdecl
#else
# define WCD_API
# define WCD_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
enum { WCD_ABI_VERSION = 1, WCD_DEFAULT_PORT = 27046, WCD_MAX_LOBBIES = 16, WCD_MAX_NAME_CHARS = 48 };
enum { WCD_EMPTY = 0, WCD_OK = 1, WCD_INVALID_ARGUMENT = -1, WCD_FAILED = -2,
       WCD_ABI_MISMATCH = -3, WCD_INTERNAL_ERROR = -4 };
#pragma pack(push, 8)
typedef struct WcdCreateArgs {
    uint32_t abiVersion, structBytes, discoveryPort, reserved;
} WcdCreateArgs;
typedef struct WcdLobby {
    uint32_t abiVersion, structBytes, ipv4, port, occupied, nameChars;
    uint16_t name[49];
    uint16_t reserved;
} WcdLobby;
// ipv4 is host order (A<<24)|(B<<16)|(C<<8)|D. name is bounded UTF16 with
// explicit code-unit count, zero tail and terminator. UDP remains <=48 UTF8 B.
typedef struct WcdStatus {
    uint32_t abiVersion, structBytes, hosting, searching, lobbyCount;
    int32_t lastSocketError;
    uint32_t ownedSockets, reserved;
    uint64_t revision;
    uint64_t socketsOpened, socketsClosed, probesSent, probesReceived, repliesSent, repliesReceived;
    uint64_t invalidDatagrams, sendErrors, receiveErrors, closeErrors;
    uint64_t wsaStarts, wsaCleanups, wsaCleanupErrors;
} WcdStatus;
typedef struct WcdAbiLayout {
    uint32_t abiVersion, structBytes, createArgsBytes, lobbyBytes, statusBytes, maximumLobbies;
} WcdAbiLayout;
#pragma pack(pop)
typedef struct WcdDiscoveryHandle WcdDiscoveryHandle;
WCD_API uint32_t WCD_CALL wc_discovery_abi_version(void);
WCD_API int32_t WCD_CALL wc_discovery_get_abi_layout(WcdAbiLayout*, uint32_t outputBytes);
// discoveryPort=0 selects27046; magic is fixed to W7Q1/W7R1 and cannot select P.
WCD_API int32_t WCD_CALL wc_discovery_create(const WcdCreateArgs*, WcdDiscoveryHandle**);
WCD_API void WCD_CALL wc_discovery_destroy(WcdDiscoveryHandle*);
WCD_API int32_t WCD_CALL wc_discovery_start_hosting(WcdDiscoveryHandle*, uint32_t actualBootstrapPort,
    const uint16_t* name, uint32_t nameChars);
WCD_API int32_t WCD_CALL wc_discovery_set_occupied(WcdDiscoveryHandle*, uint32_t occupied);
WCD_API int32_t WCD_CALL wc_discovery_begin_search(WcdDiscoveryHandle*, uint64_t nowMilliseconds);
WCD_API int32_t WCD_CALL wc_discovery_pump(WcdDiscoveryHandle*, uint64_t nowMilliseconds);
WCD_API int32_t WCD_CALL wc_discovery_status(const WcdDiscoveryHandle*, WcdStatus*, uint32_t outputBytes);
// Indexed snapshot lookup, no pop. Out-of-range returns EMPTY. Failure leaves
// output unchanged. Revision lets the UI refresh results after Pump/search end.
WCD_API int32_t WCD_CALL wc_discovery_try_get_lobby(const WcdDiscoveryHandle*, uint32_t index,
    WcdLobby*, uint32_t outputBytes);
// Closes host/search sockets and balances WSA. The serial owner may search or
// host again on this same handle; prior result snapshots remain until search.
WCD_API void WCD_CALL wc_discovery_stop(WcdDiscoveryHandle*);
#ifdef __cplusplus
}
#endif
