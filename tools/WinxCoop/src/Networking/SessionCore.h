#pragma once
// Our deterministic local session coordinator. Original Packet/MemoryStream
// provide the codec; original DGE bootstrap/StateCtrl/Worker are not emulated.
#include "TcpPacketFramer.h"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace winxcoop::networking
{
    enum class SessionRole { Host, Guest };
    enum class SessionPhase { Idle, AwaitingProbe, AwaitingInfo, AwaitingReady, WaitingForHost, Running, Closed, Failed };
    enum class SessionError { None, InvalidPacket, InvalidIdentity, UnexpectedPacket, InvalidAssignment,
                              FramingFailure, TransportFailure, Timeout, ClockMovedBackward, Backpressure,
                              ResourceFailure, CodecFailure };

    struct Endpoint
    {
        // Our explicit host-order IPv4 contract, independent of original fields.
        std::uint32_t address = 0;
        std::uint16_t port = 0;
    };
    struct SessionConfig
    {
        SessionRole role = SessionRole::Host;
        Endpoint local;
        std::optional<Endpoint> server; // Required for Guest, absent for Host.
        std::uint64_t heartbeatMilliseconds = 5000;
        std::uint64_t timeoutMilliseconds = 40000;
    };
    struct SessionMessage
    {
        std::uint32_t classId = 0, key = 0;
        std::vector<std::uint8_t> payload;
    };

    class SessionCore final
    {
    public:
        static constexpr std::uint16_t HostId = 0, GuestId = 1, UnassignedId = 0xbad0;
        static constexpr std::size_t MaximumOutboundFrames = 8, MaximumInboundMessages = 16;
        explicit SessionCore(SessionConfig);
        // The platform calls Attach only after one actual transport connection
        // is established. A host accepts one guest; further accepts are a
        // platform responsibility. No socket/thread or implicit default port.
        [[nodiscard]] bool Attach(std::uint64_t nowMilliseconds);
        // Host may authorize before connection or after Ready. Ready alone
        // never starts a campaign. Token is our explicit use of GameStart's ID.
        [[nodiscard]] bool HostStart(std::uint16_t token, std::uint64_t nowMilliseconds);
        [[nodiscard]] std::size_t Receive(const std::uint8_t*, std::size_t, std::uint64_t nowMilliseconds);
        void Tick(std::uint64_t nowMilliseconds);
        void EndOfInput(std::uint64_t nowMilliseconds);
        void FailTransport() noexcept;
        void Close() noexcept;
        [[nodiscard]] bool SendApplicationMessage(std::uint32_t classId, std::uint32_t key,
            const void*, std::uint16_t size, std::uint64_t nowMilliseconds);
        [[nodiscard]] std::optional<std::vector<std::uint8_t>> TakeOutbound();
        [[nodiscard]] std::optional<SessionMessage> TakeMessage();
        [[nodiscard]] SessionRole Role() const noexcept { return config_.role; }
        [[nodiscard]] SessionPhase Phase() const noexcept { return phase_; }
        [[nodiscard]] SessionError Error() const noexcept { return error_; }
        [[nodiscard]] std::uint16_t LocalId() const noexcept { return localId_; }
        [[nodiscard]] std::optional<std::uint16_t> StartToken() const noexcept { return startToken_; }
        [[nodiscard]] bool IsReady() const noexcept { return ready_; }
        [[nodiscard]] std::size_t OutboundCount() const noexcept { return outbound_.size(); }
        [[nodiscard]] std::size_t MessageCount() const noexcept { return incoming_.size(); }
        [[nodiscard]] std::size_t PendingBytes() const noexcept { return framer_.PendingByteCount(); }
    private:
        [[nodiscard]] bool AdvanceTime(std::uint64_t);
        [[nodiscard]] bool IsConnected() const noexcept;
        void Fail(SessionError) noexcept;
        [[nodiscard]] bool Emit(std::uint16_t type, std::uint16_t destination,
            const void*, std::uint16_t size, std::uint32_t field1 = 0, std::uint32_t field2 = 0);
        [[nodiscard]] bool EmitStart();
        void Handle(const sparkplug::reconstruction::spNetworkPacket&, std::uint64_t);
        SessionConfig config_;
        TcpPacketFramer framer_;
        SessionPhase phase_ = SessionPhase::Idle;
        SessionError error_ = SessionError::None;
        std::uint16_t localId_;
        std::optional<std::uint16_t> startToken_;
        bool ready_ = false, hasTime_ = false;
        std::uint64_t lastNow_ = 0, lastReceived_ = 0, lastPingSent_ = 0;
        std::optional<std::uint32_t> pendingPing_;
        std::deque<std::vector<std::uint8_t>> outbound_;
        std::deque<SessionMessage> incoming_;
    };
}
