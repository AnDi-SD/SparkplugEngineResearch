#pragma once
#include "ProgressionProtocol.h"
#include <deque>

namespace winxcoop::progression
{
    enum class Phase { Running, Preparing, WaitingLoad, Loading, WaitingGuest, WaitingResume, ResumePending, Closed };
    // Explicit trusted bootstrap, not evidence that the game entered this scene.
    struct TrustedInitialRunning final
    { SceneIdentity scene; LevelDescriptor descriptor; ChannelGeneration channels; };
    // These typed attestations are external native-owner contracts. The FSM
    // checks their completeness/identity; it does not turn bools into native proof.
    struct NativeRetirementReceipt final
    {
        SceneIdentity scene;
        bool originalExitReturned{}, inputRetired{}, worldRetired{}, bindingsRetired{};
        bool borrowedBaselinesRestored{}, patchesRestored{}, noNativeOwnership{};
        bool noPendingNativeCallsOrApply{}, laterWorldResourcesReleased{};
    };
    struct NativeEntryReceipt final
    {
        SceneIdentity scene; LevelDescriptor descriptor; ChannelGeneration channels;
        Digest seed{}, witness{};
        std::uint64_t localEntryOrdinal{};
        bool originalLoadReturned{}, actualReadyNativeWorld{};
    };
    struct NativeWorldAppliedReceipt final
    {
        SceneIdentity scene; ChannelGeneration channels;
        std::uint64_t revision{}, nativeTick{};
        bool completeNativeSnapshotApplied{}, freshNeutralInputBoundaryObserved{};
    };

    // Single-threaded deterministic scene barrier, no clocks, waits, threads,
    // transports or native calls. Owner supplies monotonic time on every event.
    class Session final
    {
    public:
        static constexpr std::size_t MaximumOutboundMessages = 4;
        Session(Role, ConnectionIdentity, TrustedInitialRunning, std::uint64_t transitionTimeoutMilliseconds);
        [[nodiscard]] bool Begin(const Decision&, std::uint64_t now); // Host only
        [[nodiscard]] bool ObserveLocalRetired(const NativeRetirementReceipt&, std::uint64_t now);
        [[nodiscard]] bool ObserveHostEntry(const NativeEntryReceipt&, std::uint64_t now);
        [[nodiscard]] bool ObserveGuestEntryApplied(const NativeEntryReceipt&, const NativeWorldAppliedReceipt&, std::uint64_t now);
        [[nodiscard]] bool Receive(const Message&, std::uint64_t now);
        [[nodiscard]] std::optional<Message> TakeLoadAuthorization(std::uint64_t now); // Guest, exactly once
        [[nodiscard]] bool Poll(std::uint64_t now);
        [[nodiscard]] std::optional<Message> PeekOutbound() const;
        [[nodiscard]] bool CommitOutbound(std::uint64_t now);
        void Stop(Terminal = Terminal::Disconnected) noexcept;
        [[nodiscard]] Phase State() const noexcept { return phase_; }
        [[nodiscard]] Terminal Failure() const noexcept { return terminal_; }
        [[nodiscard]] bool InputAllowed() const noexcept { return phase_ == Phase::Running && terminal_ == Terminal::None; }
        [[nodiscard]] SceneIdentity CurrentScene() const noexcept { return current_.scene; }
        [[nodiscard]] ChannelGeneration CurrentChannels() const noexcept { return current_.channels; }
        [[nodiscard]] std::uint64_t CompletedOperation() const noexcept { return completed_; }
        [[nodiscard]] std::uint64_t Deadline() const noexcept { return deadline_; }
        [[nodiscard]] std::size_t OutboundCount() const noexcept { return outbound_.size(); }
    private:
        Role role_; ConnectionIdentity connection_; TrustedInitialRunning current_;
        std::uint64_t timeout_, lastNow_=0, deadline_=0, completed_=0;
        Phase phase_=Phase::Running; Terminal terminal_=Terminal::None;
        bool hasTime_=false, prepareSent_=false, localRetired_=false, peerRetired_=false;
        bool retiredSent_=false, hostEntry_=false, loadSent_=false, loadReceived_=false, loadTaken_=false, loadedSent_=false;
        std::optional<Decision> decision_;
        std::optional<Message> load_, loaded_;
        std::optional<NativeEntryReceipt> localEntry_;
        std::deque<Message> outbound_;
        [[nodiscard]] bool Advance(std::uint64_t now);
        [[nodiscard]] bool Start(const Decision&, std::uint64_t now);
        [[nodiscard]] bool Fail(Terminal) noexcept;
        [[nodiscard]] bool Queue(const Message&);
        [[nodiscard]] bool MaybeLoad();
        [[nodiscard]] bool FreshChannels(const ChannelGeneration&) const noexcept;
        [[nodiscard]] bool MatchDecision(const Decision&);
        [[nodiscard]] Message Base(Kind) const;
        void Complete() noexcept;
    };
}
