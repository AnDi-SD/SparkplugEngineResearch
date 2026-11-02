#pragma once
#include "WorldStateProtocol.h"
#include <deque>

namespace winxcoop::world_state
{
    struct SessionIdentity final
    { std::uint64_t sessionId{},scenarioId{},hostToken{}; std::uint32_t levelEpoch{},generation{}; };
    // Single-threaded deterministic owner. It does not call a native game,
    // sleep, read clocks, or create a transport. Outbound progress commits only
    // after the existing transport accepts the complete application frame.
    class Session final
    {
    public:
        Session(Role,SessionIdentity);
        [[nodiscard]] bool ObserveActor(Availability,std::uint32_t readinessRevision);
        [[nodiscard]] bool OfferSnapshot(const Snapshot&); // Host only; absolute replacements
        [[nodiscard]] bool ObserveApplied(std::uint32_t epoch,std::uint64_t revision); // Guest only
        [[nodiscard]] bool Receive(const Message&);
        [[nodiscard]] std::optional<Message> PeekOutbound() const;
        [[nodiscard]] bool CommitOutbound();
        [[nodiscard]] std::optional<Snapshot> TakeReceivedSnapshot();
        void Stop(Terminal) noexcept;
        [[nodiscard]] Phase State() const noexcept { return phase_; }
        [[nodiscard]] Terminal Failure() const noexcept { return terminal_; }
        [[nodiscard]] std::uint32_t Epoch() const noexcept { return epoch_; }
        [[nodiscard]] std::uint64_t Token() const noexcept { return token_; }
        [[nodiscard]] std::uint64_t SentRevision() const noexcept { return sent_; }
        [[nodiscard]] std::uint64_t AppliedRevision() const noexcept { return applied_; }
        [[nodiscard]] std::uint64_t ReceivedRevision() const noexcept { return received_; }
        [[nodiscard]] std::uint64_t CoalescedUnpublished() const noexcept { return coalesced_; }
        [[nodiscard]] std::uint64_t AppliedSnapshotCount() const noexcept { return appliedCount_; }
    private:
        Role role_; SessionIdentity id_; Phase phase_=Phase::Waiting; Terminal terminal_=Terminal::None;
        bool localReady_=false,peerReady_=false,hello_=false,helloSent_=false,handshake_=false,epochActive_=false;
        bool receivedTaken_=false,waitingNative_=false;
        std::uint32_t localRevision_=0,peerRevision_=0,epoch_=0;
        std::uint64_t token_=0,sent_=0,applied_=0,received_=0,latestRevision_=0,latestTick_=0,coalesced_=0;
        std::uint64_t appliedCount_=0;
        std::vector<std::uint8_t> latest_,flight_;
        std::uint64_t flightRevision_=0; std::uint32_t flightOffset_=0;
        std::deque<Message> controls_; Assembly assembly_; std::optional<Snapshot> delivery_;
        Message Base(Kind) const;
        bool Queue(Message);
        bool Fail(Terminal) noexcept;
        void ClearEpoch() noexcept;
        bool BeginEpoch();
        bool BeginSnapshot();
    };
}
