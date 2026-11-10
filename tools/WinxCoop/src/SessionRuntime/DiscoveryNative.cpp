#define WINXCOOP_DISCOVERY_NATIVE_BUILD
#include "DiscoveryNative.h"
#include "Launcher/LobbyDiscovery.h"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <type_traits>

#if defined(_MSC_VER) && defined(_M_IX86)
#define WCD_EXPORT(name) __pragma(comment(linker, "/EXPORT:" #name "=_" #name))
WCD_EXPORT(wc_discovery_abi_version)
WCD_EXPORT(wc_discovery_get_abi_layout)
WCD_EXPORT(wc_discovery_create)
WCD_EXPORT(wc_discovery_destroy)
WCD_EXPORT(wc_discovery_start_hosting)
WCD_EXPORT(wc_discovery_set_occupied)
WCD_EXPORT(wc_discovery_begin_search)
WCD_EXPORT(wc_discovery_pump)
WCD_EXPORT(wc_discovery_status)
WCD_EXPORT(wc_discovery_try_get_lobby)
WCD_EXPORT(wc_discovery_stop)
#undef WCD_EXPORT
#endif

namespace l = winxcoop::launcher;
static_assert(sizeof(WcdCreateArgs) == 16 && sizeof(WcdLobby) == 124 && sizeof(WcdStatus) == 144);
static_assert(sizeof(WcdAbiLayout) == 24 && sizeof(wchar_t) == sizeof(uint16_t));
static_assert(std::is_standard_layout_v<WcdLobby> && std::is_trivially_copyable_v<WcdLobby>);
static_assert(WCD_MAX_LOBBIES == l::LobbyDiscovery::MaximumLobbies);

struct WcdDiscoveryHandle final
{
    l::LobbyDiscovery discovery;
    uint64_t lastNow{};
    bool hasTime{};
    explicit WcdDiscoveryHandle(uint16_t port) : discovery(Options(port)) {}
    static l::DiscoveryOptions Options(uint16_t port)
    {
        l::DiscoveryOptions result; result.port = port;
        result.probeMagic = {'W', '7', 'Q', 1}; result.replyMagic = {'W', '7', 'R', 1}; return result;
    }
    bool Advance(uint64_t now) noexcept
    { if (hasTime && now < lastNow) return false; hasTime = true; lastNow = now; return true; }
};
namespace
{
    template<class Action> int32_t Guard(WcdDiscoveryHandle* handle, Action action) noexcept
    {
        if (!handle) return WCD_INVALID_ARGUMENT;
        try { return action(handle->discovery); }
        catch (...) { handle->discovery.Close(); return WCD_INTERNAL_ERROR; }
    }
}
uint32_t WCD_CALL wc_discovery_abi_version(void) { return WCD_ABI_VERSION; }
int32_t WCD_CALL wc_discovery_get_abi_layout(WcdAbiLayout* output, uint32_t bytes)
{
    if (!output) return WCD_INVALID_ARGUMENT;
    if (bytes != sizeof(*output)) return WCD_ABI_MISMATCH;
    *output = {WCD_ABI_VERSION, sizeof(*output), sizeof(WcdCreateArgs), sizeof(WcdLobby), sizeof(WcdStatus), WCD_MAX_LOBBIES};
    return WCD_OK;
}
int32_t WCD_CALL wc_discovery_create(const WcdCreateArgs* args, WcdDiscoveryHandle** output)
{
    if (!args || !output) return WCD_INVALID_ARGUMENT;
    if (args->abiVersion != WCD_ABI_VERSION || args->structBytes != sizeof(*args)) return WCD_ABI_MISMATCH;
    if (args->reserved || args->discoveryPort > UINT16_MAX) return WCD_INVALID_ARGUMENT;
    try
    {
        *output = new WcdDiscoveryHandle(static_cast<uint16_t>(args->discoveryPort ? args->discoveryPort : WCD_DEFAULT_PORT));
        return WCD_OK;
    }
    catch (const std::invalid_argument&) { return WCD_INVALID_ARGUMENT; }
    catch (...) { return WCD_INTERNAL_ERROR; }
}
void WCD_CALL wc_discovery_destroy(WcdDiscoveryHandle* handle) { delete handle; }
int32_t WCD_CALL wc_discovery_start_hosting(WcdDiscoveryHandle* handle, uint32_t port, const uint16_t* name, uint32_t chars)
{
    if (!handle || !port || port > UINT16_MAX || !name || !chars || chars > WCD_MAX_NAME_CHARS) return WCD_INVALID_ARGUMENT;
    for (uint32_t i = 0; i < chars; ++i) if (name[i] < 32 || name[i] == 127) return WCD_INVALID_ARGUMENT;
    return Guard(handle, [&](auto& discovery) -> int32_t {
        std::wstring title; title.reserve(chars);
        for (uint32_t i = 0; i < chars; ++i) title.push_back(static_cast<wchar_t>(name[i]));
        return discovery.StartHosting(static_cast<uint16_t>(port), title) ? WCD_OK : WCD_FAILED;
    });
}
int32_t WCD_CALL wc_discovery_set_occupied(WcdDiscoveryHandle* handle, uint32_t occupied)
{
    if (occupied > 1) return WCD_INVALID_ARGUMENT;
    return Guard(handle, [&](auto& discovery) { discovery.SetOccupied(occupied != 0); return WCD_OK; });
}
int32_t WCD_CALL wc_discovery_begin_search(WcdDiscoveryHandle* handle, uint64_t now)
{
    if (!handle || !handle->Advance(now)) return WCD_INVALID_ARGUMENT;
    return Guard(handle, [&](auto& discovery) { return discovery.BeginSearch(now) ? WCD_OK : WCD_FAILED; });
}
int32_t WCD_CALL wc_discovery_pump(WcdDiscoveryHandle* handle, uint64_t now)
{
    if (!handle || !handle->Advance(now)) return WCD_INVALID_ARGUMENT;
    return Guard(handle, [&](auto& discovery) { discovery.Pump(now); return WCD_OK; });
}
int32_t WCD_CALL wc_discovery_status(const WcdDiscoveryHandle* handle, WcdStatus* output, uint32_t bytes)
{
    if (!handle || !output) return WCD_INVALID_ARGUMENT;
    if (bytes != sizeof(*output)) return WCD_ABI_MISMATCH;
    const auto& d = handle->discovery; const auto& m = d.Metrics();
    *output = {WCD_ABI_VERSION, sizeof(*output), d.Hosting(), d.Searching(), static_cast<uint32_t>(d.Lobbies().size()),
        d.LastSocketError(), static_cast<uint32_t>(d.OwnedSocketCount()), 0, d.Revision(),
        m.socketsOpened, m.socketsClosed, m.probesSent, m.probesReceived, m.repliesSent, m.repliesReceived,
        m.invalidDatagrams, m.sendErrors, m.receiveErrors, m.closeErrors, m.wsaStarts, m.wsaCleanups, m.wsaCleanupErrors};
    return WCD_OK;
}
int32_t WCD_CALL wc_discovery_try_get_lobby(const WcdDiscoveryHandle* handle, uint32_t index, WcdLobby* output, uint32_t bytes)
{
    if (!handle || !output) return WCD_INVALID_ARGUMENT;
    if (bytes != sizeof(*output)) return WCD_ABI_MISMATCH;
    const auto& lobbies = handle->discovery.Lobbies();
    if (index >= lobbies.size()) return WCD_EMPTY;
    const auto& lobby = lobbies[index];
    if (lobby.name.empty() || lobby.name.size() > WCD_MAX_NAME_CHARS) return WCD_INTERNAL_ERROR;
    WcdLobby result{WCD_ABI_VERSION, sizeof(WcdLobby), lobby.endpoint.address, lobby.endpoint.port,
        lobby.occupied, static_cast<uint32_t>(lobby.name.size()), {}, 0};
    for (size_t i = 0; i < lobby.name.size(); ++i) result.name[i] = static_cast<uint16_t>(lobby.name[i]);
    *output = result; return WCD_OK;
}
void WCD_CALL wc_discovery_stop(WcdDiscoveryHandle* handle) { if (handle) handle->discovery.Close(); }
