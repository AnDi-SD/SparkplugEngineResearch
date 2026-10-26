#pragma once
// WinxCoop's real Windows byte-stream adapter. One owner calls Pump and all
// methods serially; no worker thread or original networking service is used.
#include "SessionCore.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace winxcoop::networking
{
    enum class TcpTransportState { Idle, Listening, Connecting, Connected, Closed, Failed };
    enum class TcpCloseReason
    {
        None, LocalClose, RemoteEof, ConnectFailure, ConnectTimeout,
        SocketFailure, SessionFailure, ResourceFailure, ClockMovedBackward, ShutdownTimeout
    };

    struct TcpTransportConfig
    {
        SessionRole role = SessionRole::Host;
        // Wildcard address and port0 are allowed for OS bind. SessionCore is
        // created later with the concrete connected endpoint from getsockname.
        Endpoint localBind;
        std::optional<Endpoint> server;
        std::uint64_t heartbeatMilliseconds = 5000;
        std::uint64_t timeoutMilliseconds = 40000;
        std::uint64_t connectTimeoutMilliseconds = 5000;
        // These cap actual OS request sizes; they never invent short results.
        std::size_t maxSendChunk = 65536;
        std::size_t maxReceiveChunk = 4096;
        std::size_t maxIoCallsPerPump = 64;
        // Optional actual SO_SNDBUF/SO_RCVBUF requests for bounded diagnostics.
        int sendBufferBytes = 0, receiveBufferBytes = 0;
    };

    struct TcpTransportMetrics
    {
        std::uint64_t wsaStarts = 0, wsaCleanups = 0, wsaCleanupErrors = 0;
        std::uint64_t socketsOpened = 0, socketsClosed = 0, socketCloseErrors = 0;
        std::uint64_t connections = 0, accepted = 0, refused = 0;
        std::uint64_t sendCalls = 0, receiveCalls = 0, sentBytes = 0, receivedBytes = 0;
        std::uint64_t sendWouldBlock = 0, receiveWouldBlock = 0;
        std::uint64_t nativeShortSendResults = 0, framePartialAdvances = 0;
        std::uint64_t sendRequestCapped = 0, receivePausedForInbox = 0;
        std::uint64_t remoteEofs = 0, socketErrors = 0;
        std::uint64_t droppedSessionEvents = 0;
        std::uint64_t applicationSendBackpressure = 0;
        std::size_t peakOwnedSockets = 0;
    };
    struct TcpTerminal
    {
        std::uint64_t connectionId = 0;
        TcpCloseReason reason = TcpCloseReason::None;
        int socketError = 0;
        SessionPhase sessionPhase = SessionPhase::Idle;
        SessionError sessionError = SessionError::None;
    };
    struct TcpReceivedMessage
    {
        // This association survives reconnect. Caller must not apply an old
        // connection's message to the new connection's game/session state.
        std::uint64_t connectionId = 0;
        SessionMessage message;
    };
    struct TcpSessionEvent
    {
        std::uint64_t connectionId = 0, nowMilliseconds = 0;
        SessionPhase sessionPhase = SessionPhase::Idle;
        SessionError sessionError = SessionError::None;
        std::optional<std::uint16_t> startToken;
    };

    class WindowsTcpTransport final
    {
    public:
        static constexpr std::size_t MaximumReceiveBuffer = 65536;
        static constexpr std::size_t MaximumDeliveredMessages = 16;
        static constexpr std::size_t MaximumSessionEvents = 64;
        // Leave one Core slot for heartbeat/control while OS writes stall.
        static constexpr std::size_t ApplicationQueueLimit = SessionCore::MaximumOutboundFrames - 1;
        explicit WindowsTcpTransport(TcpTransportConfig);
        ~WindowsTcpTransport();
        WindowsTcpTransport(const WindowsTcpTransport&) = delete;
        WindowsTcpTransport& operator=(const WindowsTcpTransport&) = delete;
        WindowsTcpTransport(WindowsTcpTransport&&) = delete;
        WindowsTcpTransport& operator=(WindowsTcpTransport&&) = delete;

        [[nodiscard]] bool Start(std::uint64_t nowMilliseconds);
        void Pump(std::uint64_t nowMilliseconds);
        [[nodiscard]] bool HostStart(std::uint16_t token, std::uint64_t nowMilliseconds);
        [[nodiscard]] bool SendApplicationMessage(std::uint64_t expectedConnectionId,
            std::uint32_t classId, std::uint32_t key, const void*, std::uint16_t size,
            std::uint64_t nowMilliseconds);
        [[nodiscard]] std::optional<TcpReceivedMessage> TakeMessage();
        [[nodiscard]] std::optional<TcpSessionEvent> TakeSessionEvent() noexcept;
        // Only after every queued byte was handed to send. Keeps reading until
        // peer EOF/error; new application sends/heartbeats are then rejected.
        // The half-close wait is bounded by timeoutMilliseconds.
        [[nodiscard]] bool ShutdownSend();
        // Host listener survives peer closure and admits a new SessionCore.
        // Abortive close is explicit and uses actual SO_LINGER{1,0} for RST.
        void ClosePeer(bool abortive = false) noexcept;
        // Releases sockets before balancing WSAStartup. Terminal object cannot
        // restart; construct a new transport for a new listener/client service.
        void Close() noexcept;

        [[nodiscard]] TcpTransportState State() const noexcept;
        [[nodiscard]] Endpoint BoundEndpoint() const noexcept;
        [[nodiscard]] std::optional<Endpoint> PeerEndpoint() const noexcept;
        [[nodiscard]] const SessionCore* Session() const noexcept;
        [[nodiscard]] std::uint64_t ActiveConnectionId() const noexcept;
        [[nodiscard]] std::optional<TcpTerminal> LastTerminal() const noexcept;
        [[nodiscard]] int LastSocketError() const noexcept;
        [[nodiscard]] const TcpTransportMetrics& Metrics() const noexcept;
        [[nodiscard]] std::size_t OwnedSocketCount() const noexcept;
        [[nodiscard]] std::size_t PendingSendBytes() const noexcept;
        [[nodiscard]] std::size_t PendingReceiveBytes() const noexcept;
        [[nodiscard]] std::size_t DeliveredMessageCount() const noexcept;
    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
