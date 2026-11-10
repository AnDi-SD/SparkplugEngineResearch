#pragma once
#include "BootstrapProtocol.h"
#include "Networking/WindowsTcpTransport.h"
#include <array>
#include <deque>

namespace winxcoop::bootstrap
{
    constexpr std::size_t MaximumControlRecords = 4, ControlBytes = 244;
    struct Config final
    {
        Role role = Role::Host;
        networking::TcpTransportConfig transport;
        Digest profile{};
        std::uint64_t transferTimeoutMilliseconds = 120000;
    };
    enum class Admission { Accepted, Full, Failed };
    struct Status final
    {
        Phase phase = Phase::Idle;
        Terminal terminal = Terminal::None;
        Role role = Role::Host;
        bool profileMatched{}, bootstrapAvailable{}, bootstrapAcknowledged{};
        networking::Endpoint bound{}, peer{};
        std::uint64_t connectionId{}, transferId{}, totalBytes{}, receivedBytes{};
        std::uint32_t controlQueued{}, pendingOutbound{}, ownedSockets{};
        std::uint64_t socketsOpened{}, socketsClosed{}, wsaStarts{}, wsaCleanups{};
        std::uint64_t lastPumpMilliseconds{}, lastProgressMilliseconds{};
        Assignment assignment;
    };

    // One TCP peer per process/role, no native game/process/mapping ownership.
    // Keeps one immutable transfer and at most four control records each way.
    // Caller supplies its own local monotonic milliseconds on every mutation.
    class Peer final
    {
    public:
        explicit Peer(Config);
        ~Peer();
        Peer(const Peer&) = delete;
        Peer& operator=(const Peer&) = delete;
        [[nodiscard]] bool Start(std::uint64_t now);
        [[nodiscard]] bool Pump(std::uint64_t now);
        [[nodiscard]] bool Publish(Assignment, const std::vector<std::uint8_t>&, std::uint64_t now);
        [[nodiscard]] bool HasBootstrap() const noexcept;
        [[nodiscard]] const std::vector<std::uint8_t>& BootstrapBytes() const noexcept { return payload_; }
        [[nodiscard]] const Assignment& CurrentAssignment() const noexcept { return assignment_; }
        [[nodiscard]] bool ConsumeBootstrap() noexcept;
        [[nodiscard]] Admission SendControl(const void*, std::size_t bytes, std::uint64_t now);
        [[nodiscard]] bool HasControl() const noexcept { return !receivedControl_.empty(); }
        [[nodiscard]] const std::array<std::uint8_t, ControlBytes>& FrontControl() const;
        void ConsumeControl() noexcept;
        [[nodiscard]] Status Snapshot() const noexcept;
        [[nodiscard]] bool ObserveReady(const ReadyReceipt&, std::uint64_t now);
        [[nodiscard]] bool LocalReady() const noexcept { return localReady_; }
        [[nodiscard]] bool RemoteReady() const noexcept { return remoteReady_; }
        [[nodiscard]] bool BothReady() const noexcept { return terminal_ == Terminal::None && localReady_ && remoteReady_; }
        [[nodiscard]] const ReadyReceipt& PeerReadyReceipt() const noexcept { return remoteReadyReceipt_; }
        [[nodiscard]] Admission SendNotice(const OwnerNotice&, std::uint64_t now);
        [[nodiscard]] bool HasNotice() const noexcept { return !receivedNotices_.empty(); }
        [[nodiscard]] const OwnerNotice& FrontNotice() const;
        void ConsumeNotice() noexcept;
        [[nodiscard]] Terminal Failure() const noexcept { return terminal_; }
        void Stop(Terminal = Terminal::LocalStop) noexcept;
    private:
        Config config_;
        networking::WindowsTcpTransport transport_;
        bool started_{}, hasTime_{}, helloSent_{}, profileMatched_{};
        bool haveAssignment_{}, offerSent_{}, commitSent_{}, acknowledged_{};
        bool receiveComplete_{}, consumed_{}, acknowledgmentPending_{};
        bool localReady_{}, remoteReady_{}, readySent_{};
        bool transferStartedValid_{}, readyStartedValid_{};
        ReadyReceipt localReadyReceipt_{}, remoteReadyReceipt_{};
        std::uint64_t readyStarted_{};
        std::uint64_t connection_{}, lastNow_{}, lastProgress_{}, handshakeStarted_{}, transferStarted_{}, transferId_{};
        std::uint32_t sentOffset_{}, receivedOffset_{};
        Terminal terminal_ = Terminal::None;
        Assignment assignment_{};
        std::vector<std::uint8_t> payload_;
        std::deque<networking::SessionMessage> outgoingControl_;
        std::deque<std::array<std::uint8_t, ControlBytes>> receivedControl_;
        std::deque<OwnerNotice> outgoingNotices_, receivedNotices_;
        std::array<std::optional<OwnerNotice>, 5> localNotices_{}, remoteNotices_{};
        [[nodiscard]] bool Host() const noexcept { return config_.role == Role::Host; }
        [[nodiscard]] bool Advance(std::uint64_t now);
        [[nodiscard]] bool Fail(Terminal) noexcept;
        [[nodiscard]] bool Receive(const networking::TcpReceivedMessage&, std::uint64_t now);
        [[nodiscard]] bool ReceiveBootstrap(const Message&, std::uint64_t now);
        [[nodiscard]] bool SendBootstrap(const Message&, std::uint64_t now);
        [[nodiscard]] bool SendPending(std::uint64_t now);
        [[nodiscard]] bool IsNext(const Assignment&) const noexcept;
        [[nodiscard]] static std::optional<networking::SessionMessage> ControlRecord(const void*, std::size_t);
    };
}
