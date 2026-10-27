#pragma once

// WinxCoop's bounded LAN advertisement protocol. This is not the recovered
// Sparkplug Packet protocol; it advertises an endpoint for that TCP carrier.
#include "Networking/SessionCore.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace winxcoop::launcher
{
    struct DiscoveredLobby
    {
        networking::Endpoint endpoint;
        std::wstring name;
        bool occupied = false;
    };

    struct DiscoveryMetrics
    {
        std::uint64_t socketsOpened = 0, socketsClosed = 0;
        std::uint64_t probesSent = 0, probesReceived = 0, repliesSent = 0, repliesReceived = 0;
        std::uint64_t invalidDatagrams = 0, sendErrors = 0, receiveErrors = 0, closeErrors = 0;
        std::uint64_t wsaStarts = 0, wsaCleanups = 0, wsaCleanupErrors = 0;
    };

    class LobbyDiscovery final
    {
    public:
        static constexpr std::uint16_t DiscoveryPort = 27026;
        static constexpr std::size_t MaximumLobbies = 16;
        static constexpr std::uint64_t SearchMilliseconds = 2500;
        LobbyDiscovery();
        ~LobbyDiscovery();
        LobbyDiscovery(const LobbyDiscovery&) = delete;
        LobbyDiscovery& operator=(const LobbyDiscovery&) = delete;

        // One UI/network owner calls all methods serially. Hosting/search
        // sockets are separate and own their own bound endpoint.
        [[nodiscard]] bool StartHosting(std::uint16_t tcpPort, const std::wstring& name);
        void StopHosting() noexcept;
        void SetOccupied(bool) noexcept;
        [[nodiscard]] bool BeginSearch(std::uint64_t nowMilliseconds);
        void Pump(std::uint64_t nowMilliseconds);
        void Close() noexcept;
        [[nodiscard]] bool Hosting() const noexcept;
        [[nodiscard]] bool Searching() const noexcept;
        [[nodiscard]] int LastSocketError() const noexcept;
        [[nodiscard]] std::uint64_t Revision() const noexcept;
        [[nodiscard]] const std::vector<DiscoveredLobby>& Lobbies() const noexcept;
        [[nodiscard]] const DiscoveryMetrics& Metrics() const noexcept;
    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
