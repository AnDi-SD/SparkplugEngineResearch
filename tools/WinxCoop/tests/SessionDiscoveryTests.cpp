// Actual directed UDP on this computer; this is a discovery/ABI contract test,
// not a second-machine LAN or original-game qualification.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Launcher/LobbyDiscovery.h"
#include "SessionRuntime/DiscoveryNative.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace l = winxcoop::launcher;
namespace
{
    void Require(bool condition, const char* why) { if (!condition) throw std::runtime_error(why); }
    void Ok(int32_t result, const char* why) { Require(result == WCD_OK, why); }
    template<class Action, class Predicate> void Until(Action action, Predicate predicate)
    {
        const auto deadline = GetTickCount64() + 2000;
        do { action(); if (predicate()) return; Sleep(1); } while (GetTickCount64() < deadline);
        throw std::runtime_error("bounded directed UDP deadline");
    }
    struct Datagram final { std::vector<unsigned char> bytes; sockaddr_in from{}; };
    struct RawSocket final
    {
        SOCKET socket = INVALID_SOCKET;
        RawSocket()
        {
            socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); Require(socket != INVALID_SOCKET, "raw UDP socket");
            u_long nonblocking = 1; sockaddr_in address{}; address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(0x7f000001u);
            if (ioctlsocket(socket, FIONBIO, &nonblocking) == SOCKET_ERROR ||
                bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
            { Close(); throw std::runtime_error("raw UDP loopback bind"); }
        }
        ~RawSocket() { Close(); }
        RawSocket(const RawSocket&) = delete; RawSocket& operator=(const RawSocket&) = delete;
        void Close() noexcept { if (socket != INVALID_SOCKET) { closesocket(socket); socket = INVALID_SOCKET; } }
        uint16_t Port() const
        {
            sockaddr_in address{}; int bytes = sizeof(address);
            Require(getsockname(socket, reinterpret_cast<sockaddr*>(&address), &bytes) == 0 && address.sin_family == AF_INET, "real UDP endpoint");
            return ntohs(address.sin_port);
        }
        void Send(const sockaddr_in& destination, const std::vector<unsigned char>& bytes)
        {
            Require(sendto(socket, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0,
                reinterpret_cast<const sockaddr*>(&destination), sizeof(destination)) == static_cast<int>(bytes.size()), "actual UDP full datagram send");
        }
        std::optional<Datagram> Receive()
        {
            Datagram value; value.bytes.resize(256); int addressBytes = sizeof(value.from);
            const auto count = recvfrom(socket, reinterpret_cast<char*>(value.bytes.data()), static_cast<int>(value.bytes.size()), 0,
                reinterpret_cast<sockaddr*>(&value.from), &addressBytes);
            if (count == SOCKET_ERROR) { Require(WSAGetLastError() == WSAEWOULDBLOCK, "actual UDP receive error"); return std::nullopt; }
            Require(addressBytes == sizeof(value.from) && value.from.sin_family == AF_INET, "received UDP source");
            value.bytes.resize(static_cast<size_t>(count)); return value;
        }
    };
    uint16_t AvailablePort() { RawSocket socket; return socket.Port(); }
    l::DiscoveryOptions RuntimeOptions(uint16_t port)
    {
        l::DiscoveryOptions options; options.port = port; options.probeAddress = 0x7f000001u;
        options.probeMagic = {'W', '7', 'Q', 1}; options.replyMagic = {'W', '7', 'R', 1}; return options;
    }
    struct NativeOwner final
    {
        WcdDiscoveryHandle* handle{};
        explicit NativeOwner(uint32_t port)
        {
            const WcdCreateArgs args{WCD_ABI_VERSION, sizeof(WcdCreateArgs), port, 0};
            Ok(wc_discovery_create(&args, &handle), "native discovery create"); Require(handle != nullptr, "native discovery handle");
        }
        ~NativeOwner() { wc_discovery_destroy(handle); }
        NativeOwner(const NativeOwner&) = delete; NativeOwner& operator=(const NativeOwner&) = delete;
        WcdStatus Status() const
        { WcdStatus status{}; Ok(wc_discovery_status(handle, &status, sizeof(status)), "native discovery status"); return status; }
        void Pump() { Ok(wc_discovery_pump(handle, GetTickCount64()), "native discovery pump"); }
        void Stop()
        {
            wc_discovery_stop(handle); const auto status = Status();
            Require(!status.hosting && !status.searching && !status.ownedSockets && status.socketsOpened == status.socketsClosed &&
                status.wsaStarts == status.wsaCleanups && !status.closeErrors && !status.wsaCleanupErrors, "native stop balances every discovery resource");
        }
    };
    void Closed(l::LobbyDiscovery& discovery)
    {
        discovery.Close(); const auto& metrics = discovery.Metrics();
        Require(!discovery.OwnedSocketCount() && metrics.socketsOpened == metrics.socketsClosed && metrics.wsaStarts == metrics.wsaCleanups &&
            !metrics.closeErrors && !metrics.wsaCleanupErrors, "shared discovery balances every resource");
    }
    void AbiAndDefaults()
    {
        const l::DiscoveryOptions defaults;
        Require(defaults.port == 27026 && defaults.probeAddress == 0xffffffffu &&
            defaults.probeMagic == std::array<unsigned char, 4>{'W','C','Q',1} &&
            defaults.replyMagic == std::array<unsigned char, 4>{'W','C','R',1}, "Stage P defaults preserved");
        Require(WCD_DEFAULT_PORT == 27046 && wc_discovery_abi_version() == 1, "separate E7 domain ABI");
        WcdAbiLayout layout{}; Ok(wc_discovery_get_abi_layout(&layout, sizeof(layout)), "discovery layout export");
        Require(layout.createArgsBytes == 16 && layout.lobbyBytes == 124 && layout.statusBytes == 144 &&
            layout.maximumLobbies == 16, "actual discovery layout");
        const auto copy = layout;
        Require(wc_discovery_get_abi_layout(&layout, 1) == WCD_ABI_MISMATCH && std::memcmp(&copy, &layout, sizeof(copy)) == 0,
            "ABI mismatch leaves output untouched");
        WcdDiscoveryHandle* output = reinterpret_cast<WcdDiscoveryHandle*>(static_cast<uintptr_t>(1));
        WcdCreateArgs args{WCD_ABI_VERSION, sizeof(args), 65536, 0};
        Require(wc_discovery_create(&args, &output) == WCD_INVALID_ARGUMENT && output == reinterpret_cast<WcdDiscoveryHandle*>(static_cast<uintptr_t>(1)),
            "invalid port does not truncate or mutate handle output");
        args.discoveryPort = 0; args.abiVersion = 99;
        Require(wc_discovery_create(&args, &output) == WCD_ABI_MISMATCH, "create ABI gate");
        NativeOwner owner(0); auto status = owner.Status(); Require(!status.hosting && !status.searching && !status.ownedSockets, "idle native owner");
        WcdLobby lobby{}; lobby.ipv4 = 123;
        Require(wc_discovery_try_get_lobby(owner.handle, 0, &lobby, sizeof(lobby)) == WCD_EMPTY && lobby.ipv4 == 123,
            "empty lookup leaves complete lobby output unchanged");
        Require(wc_discovery_set_occupied(owner.handle, 2) == WCD_INVALID_ARGUMENT && wc_discovery_pump(nullptr, 0) == WCD_INVALID_ARGUMENT,
            "invalid boolean/null rejected");
        const auto now = GetTickCount64(); Ok(wc_discovery_begin_search(owner.handle, now), "native bounded search open");
        Require(owner.Status().searching && owner.Status().ownedSockets == 1, "actual search socket owned");
        Require(wc_discovery_pump(owner.handle, now - 1) == WCD_INVALID_ARGUMENT && owner.Status().searching, "backward ABI clock rejected without false search completion");
        Ok(wc_discovery_pump(owner.handle, now + 2500), "native search expiry without emitting probe");
        Require(!owner.Status().searching && !owner.Status().ownedSockets, "bounded search expiry closes its socket");
        owner.Stop();
    }
    void SharedSearchAgainstNativeHost()
    {
        const auto port = AvailablePort(); NativeOwner host(port);
        const std::wstring title = L"Winx \u0411\u043b\u0443\u043c";
        std::vector<uint16_t> name(title.begin(), title.end());
        Ok(wc_discovery_start_hosting(host.handle, 27991, name.data(), static_cast<uint32_t>(name.size())), "native Host advertises supplied TCP endpoint");
        Require(host.Status().hosting && host.Status().ownedSockets == 1, "native Host socket owned");
        l::LobbyDiscovery search(RuntimeOptions(port)); Require(search.BeginSearch(GetTickCount64()), "directed loopback search");
        Until([&] { search.Pump(GetTickCount64()); host.Pump(); }, [&] { return !search.Lobbies().empty(); });
        Require(search.Lobbies().size() == 1 && search.Lobbies()[0].endpoint.address == 0x7f000001u &&
            search.Lobbies()[0].endpoint.port == 27991 && search.Lobbies()[0].name == title && !search.Lobbies()[0].occupied,
            "real E7 UDP preserves endpoint and UTF16/UTF8 title");
        Require(host.Status().probesReceived > 0 && host.Status().repliesSent > 0 && search.Metrics().repliesReceived > 0,
            "actual send/receive counters prove both directions"); Closed(search);
        Ok(wc_discovery_set_occupied(host.handle, 1), "native occupied flag");
        l::LobbyDiscovery occupied(RuntimeOptions(port)); Require(occupied.BeginSearch(GetTickCount64()), "occupied search");
        Until([&] { occupied.Pump(GetTickCount64()); host.Pump(); }, [&] { return !occupied.Lobbies().empty(); });
        Require(occupied.Lobbies()[0].occupied && occupied.Lobbies()[0].endpoint.port == 27991, "occupied flag travels over actual UDP"); Closed(occupied);
        const auto invalidBefore = host.Status().invalidDatagrams;
        l::DiscoveryOptions oldDomain; oldDomain.port = port; oldDomain.probeAddress = 0x7f000001u;
        l::LobbyDiscovery wrong(oldDomain); const auto wrongStart = GetTickCount64(); Require(wrong.BeginSearch(wrongStart), "P-domain directed negative search");
        Until([&] { wrong.Pump(GetTickCount64()); host.Pump(); }, [&] { return host.Status().invalidDatagrams > invalidBefore; });
        wrong.Pump(wrongStart + l::LobbyDiscovery::SearchMilliseconds);
        Require(wrong.Lobbies().empty() && !wrong.Searching(), "runtime Host rejects P probes on same actual port"); Closed(wrong);
        const std::array<uint16_t, 2> badName{'x', 0};
        Require(wc_discovery_start_hosting(host.handle, 27991, badName.data(), 2) == WCD_INVALID_ARGUMENT && host.Status().hosting,
            "invalid ABI name rejected before mutating existing advertisement");
        host.Stop();
    }
    void SearchRejectsWrongReplyDomain()
    {
        RawSocket responder; l::LobbyDiscovery search(RuntimeOptions(responder.Port()));
        Require(search.BeginSearch(GetTickCount64()), "reply-domain search start");
        std::optional<Datagram> probe;
        Until([&] { search.Pump(GetTickCount64()); probe = responder.Receive(); }, [&] { return probe.has_value(); });
        Require(probe->bytes.size() == 8 && std::equal(probe->bytes.begin(), probe->bytes.begin() + 4,
            std::array<unsigned char, 4>{'W','7','Q',1}.begin()), "actual runtime probe domain");
        // Wire fixture deliberately uses the existing advertisement layout;
        // it is not a second production parser/serializer.
        std::vector<unsigned char> reply{'W','C','R',1,0,0,0,0,0x56,0x34,1,0,'A'};
        std::copy_n(probe->bytes.begin() + 4, 4, reply.begin() + 4);
        responder.Send(probe->from, reply);
        Until([&] { search.Pump(GetTickCount64()); }, [&] { return search.Metrics().invalidDatagrams != 0; });
        Require(search.Lobbies().empty(), "wrong reply domain cannot enter lobby list");
        reply[1] = '7'; responder.Send(probe->from, reply);
        Until([&] { search.Pump(GetTickCount64()); }, [&] { return search.Lobbies().size() == 1; });
        Require(search.Lobbies()[0].endpoint.port == 0x3456 && search.Lobbies()[0].name == L"A", "same-source correct reply accepted after rejected foreign domain");
        Closed(search);
    }
}
int main()
{
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2,2), &data) != 0) return 1;
    try
    {
        AbiAndDefaults(); SharedSearchAgainstNativeHost(); SearchRejectsWrongReplyDomain();
        Require(WSACleanup() == 0, "test socket runtime cleanup");
        std::cout << "WinxCoop discovery ABI/directed UDP/domain/ownership contracts PASS\n"; return 0;
    }
    catch (const std::exception& error) { (void)WSACleanup(); std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
