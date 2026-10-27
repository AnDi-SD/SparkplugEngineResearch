#pragma once
// WinxCoop-owned gameplay authorization and payload format. This is not an
// original-game simulator. SessionCore remains the only Packet wire codec.
#include "../Networking/SessionCore.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace winxcoop::gameplay
{
    using Digest256 = std::array<std::uint8_t, 32>;
    struct Compatibility
    {
        Digest256 executable{}, adapter{};
    };
    struct LevelDescriptor
    {
        std::uint32_t levelId = 0, entryId = 0;
        Digest256 resources{}, scenario{};
    };
    [[nodiscard]] bool operator==(const Compatibility&, const Compatibility&) noexcept;
    [[nodiscard]] bool operator==(const LevelDescriptor&, const LevelDescriptor&) noexcept;

    enum class MessageKind : std::uint8_t
    { Hello = 1, Welcome, Ready, Resume, Input, State, Remove };
    enum class Phase
    { Idle, AwaitingHello, AwaitingAssignment, WaitingForHostLevel, Loading, Running, Closed, Failed };
    enum class Error
    { None, InvalidMessage, Incompatible, WrongRole, WrongPhase, InvalidEpoch, Backpressure, ResourceFailure, SequenceExhausted, InputDiscontinuity };
    enum class ReceiveResult { Accepted, IgnoredStale, WrongConnection, Rejected };
    enum class Animation : std::uint16_t { Idle, Walk, Jump };
    enum class EventKind { LevelAssigned, Started, GuestInput, AcceptedState, EntityRemoved, Disconnected };

    struct InputIntent
    {
        // V2 little-endian body: tick u32, magnitude/relative/reference f32,
        // jump-held flags u16 (bit0 only), reserved u16=0. Body20, payload44.
        static constexpr float MaximumRadians = 3.1415927410125732421875f;
        // Native command04/08/14, not Cartesian axes or an accepted pose.
        // The wire accepts finite magnitude [0,1] and angles [-pi,+pi]. This
        // numeric envelope does not qualify analog input: current original-PC
        // evidence is Alfea28, InGame device context3, keyboard magnitude0/1,
        // eight-way direction08 and request-held byte1A. An adapter must verify
        // that native input/owner/scene gate before producing these values.
        float magnitude = 0, relativeRadians = 0, referenceHeading = 0;
        bool jumpHeld = false;
        // Nonzero, nonwrapping native InputRET observation counter. This is
        // an ordering/time-difference label, not milliseconds or a host tick.
        std::uint32_t clientTick = 0;
    };
    struct AcceptedState
    {
        // acknowledgedInput is the transition sequence actually applied by the
        // trusted native host backend, never an acknowledgement of decoding.
        std::uint32_t entity = 0, hostTick = 0, acknowledgedInput = 0;
        std::array<float, 3> position{}, velocity{};
        // Native Node orientation is a 3x3 matrix, not a quaternion. Scale is
        // supplied by the qualified scene and cannot be commanded by a peer.
        std::array<float, 9> rotation{{1,0,0, 0,1,0, 0,0,1}};
        Animation animation = Animation::Idle;
    };
    struct Event
    {
        EventKind kind = EventKind::Disconnected;
        std::uint64_t session = 0;
        std::uint32_t epoch = 0, sequence = 0, entity = 0;
        LevelDescriptor level;
        InputIntent input;
        AcceptedState state;
        std::uint16_t removalReason = 0;
    };
    struct Config
    {
        networking::SessionRole role = networking::SessionRole::Host;
        Compatibility compatibility;
        // One explicitly qualified situation per instance. A matching level ID
        // alone is insufficient: both resource and scenario hashes must match.
        LevelDescriptor qualifiedLevel;
        // Nonzero, fresh per connection for Host; zero for Guest before Welcome.
        std::uint64_t hostSession = 0;
    };

    class GameplayProtocol final
    {
    public:
        static constexpr std::uint32_t ClassId = 0x31504357; // WCP1 family tag, carrier type 2; version is explicit below
        static constexpr std::uint16_t Version = 2;
        static constexpr std::uint32_t HostEntity = 1, GuestEntity = 2;
        static constexpr std::size_t MaximumPayload = 256, MaximumOutbound = 8, MaximumEvents = 16;
        explicit GameplayProtocol(Config);
        // Begin only after a real connection can carry application messages.
        // Transport GameStart authorizes type 2, not gameplay simulation.
        [[nodiscard]] bool Begin(std::uint64_t connectionId);
        // Trusted host calls this only after campaign start/current scene is
        // actually ready. No guest API chooses a level or starts gameplay.
        [[nodiscard]] bool HostAssignLevel(std::uint32_t epoch, const LevelDescriptor& actualReadyLevel);
        [[nodiscard]] bool NotifyLocalReady(const LevelDescriptor& actualReadyLevel);
        // Call on every qualified native InputRET, with a consecutive clientTick
        // after the first nonzero observation. A forward gap means upstream
        // observation loss and fails closed before any identical-state elision.
        // Only numerically identical
        // full states (including jumpHeld) are elided; the first tick of each
        // queued transition stays intact. A press and release always remain
        // separate FIFO events, even before one network send. The first event
        // is a held-state baseline, not a synthesized jump press.
        // Both queues are bounded and fail closed on overflow. The host native
        // backend must apply the ordered events with their original tick gaps;
        // it must not drain to latest state or replay late presses after landing.
        // This codec preserves labels/order only; cross-machine observation
        // clock mapping and native application/lateness remain unqualified here.
        // If it cannot honor that temporal contract, stop the connection rather
        // than guess native simulation. That backend is a separate runtime gate.
        [[nodiscard]] bool SubmitInput(const InputIntent&);
        // Only a trusted local host backend supplies accepted simulation state.
        // Input events must be simulated/validated there; receiving them never
        // produces an accepted transform or proves native controller behavior.
        [[nodiscard]] bool PublishAcceptedState(const AcceptedState&);
        [[nodiscard]] bool RemoveEntity(std::uint32_t entity, std::uint16_t reason = 0);
        [[nodiscard]] ReceiveResult Receive(std::uint64_t connectionId, const networking::SessionMessage&);
        // For a transport with backpressure, peek then commit ONLY after it
        // accepts the whole application message. A failed send leaves the same
        // front message intact. Borrowed pointer lasts until the next mutation.
        [[nodiscard]] const networking::SessionMessage* PeekOutbound() const noexcept;
        [[nodiscard]] bool CommitOutbound() noexcept;
        // Alternative ownership transfer: caller retains/retries this exact
        // message before taking/sending another; it must never drop an input.
        [[nodiscard]] std::optional<networking::SessionMessage> TakeOutbound();
        [[nodiscard]] std::optional<Event> TakeEvent();
        // One terminal instance per connection. Close invalidates undelivered
        // input/state and emits at most one remote removal plus Disconnected.
        void Close() noexcept;
        [[nodiscard]] Phase CurrentPhase() const noexcept { return phase_; }
        [[nodiscard]] Error LastError() const noexcept { return error_; }
        [[nodiscard]] std::uint64_t ConnectionId() const noexcept { return connection_; }
        [[nodiscard]] std::uint64_t SessionId() const noexcept { return session_; }
        [[nodiscard]] std::uint32_t LevelEpoch() const noexcept { return epoch_; }
        [[nodiscard]] bool LocalReady() const noexcept { return localReady_; }
        [[nodiscard]] bool PeerReady() const noexcept { return peerReady_; }
        [[nodiscard]] bool EntityAlive(std::uint32_t entity) const noexcept;
        // Host: last received transition. Guest: last enqueued transition.
        // This counter is not proof that an input has been simulated/applied.
        [[nodiscard]] std::uint32_t LastGuestInputSequence() const noexcept { return lastGuestInput_; }
        [[nodiscard]] std::size_t OutboundCount() const noexcept { return outbound_.size(); }
        [[nodiscard]] std::size_t EventCount() const noexcept { return events_.size(); }
    private:
        struct Header;
        [[nodiscard]] bool Emit(MessageKind, std::uint32_t entity, const void* body, std::size_t size);
        [[nodiscard]] bool Push(Event);
        [[nodiscard]] bool SendWelcome();
        [[nodiscard]] bool SendResume();
        void Fail(Error) noexcept;
        [[nodiscard]] bool Terminal() const noexcept;
        [[nodiscard]] Event MakeEvent(EventKind, std::uint32_t entity = 0, std::uint32_t sequence = 0) const;
        [[nodiscard]] ReceiveResult Handle(const Header&, const std::uint8_t* body, std::size_t bodySize);
        Config config_;
        Phase phase_ = Phase::Idle;
        Error error_ = Error::None;
        std::uint64_t connection_ = 0, session_ = 0;
        std::uint32_t epoch_ = 0;
        bool helloReceived_ = false, assigned_ = false, welcomeSent_ = false;
        bool localReady_ = false, peerReady_ = false;
        std::array<bool, 3> alive_{};
        // Independent monotonically increasing, nonwrapping counters for each
        // message kind and entity. A new instance is required before exhaustion.
        std::array<std::array<std::uint32_t, 3>, 8> sent_{}, received_{};
        std::array<std::uint32_t, 3> stateTicks_{}, stateAcknowledgements_{};
        std::uint32_t lastGuestInput_ = 0, lastInputTick_ = 0;
        std::optional<InputIntent> lastInput_;
        std::deque<networking::SessionMessage> outbound_;
        std::deque<Event> events_;
    };
}
