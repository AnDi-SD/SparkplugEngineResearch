#include "GameplayProtocol.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace winxcoop::gameplay
{
    namespace
    {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
            "Gameplay payloads require IEEE-754 binary32");
        constexpr std::size_t HeaderSize = 24;
        constexpr std::size_t CompatibilitySize = 64, LevelSize = 72;
        void Put16(std::uint8_t* p, const std::uint16_t v)
        { p[0] = static_cast<std::uint8_t>(v); p[1] = static_cast<std::uint8_t>(v >> 8); }
        void Put32(std::uint8_t* p, const std::uint32_t v)
        { for(unsigned n = 0; n < 4; ++n) p[n] = static_cast<std::uint8_t>(v >> (n * 8)); }
        void Put64(std::uint8_t* p, const std::uint64_t v)
        { for(unsigned n = 0; n < 8; ++n) p[n] = static_cast<std::uint8_t>(v >> (n * 8)); }
        std::uint16_t Get16(const std::uint8_t* p)
        { return static_cast<std::uint16_t>(std::uint16_t(p[0]) | (std::uint16_t(p[1]) << 8)); }
        std::uint32_t Get32(const std::uint8_t* p)
        { return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24); }
        std::uint64_t Get64(const std::uint8_t* p)
        { std::uint64_t v = 0; for(unsigned n = 0; n < 8; ++n) v |= std::uint64_t(p[n]) << (n * 8); return v; }
        void PutFloat(std::uint8_t* p, const float v)
        { std::uint32_t bits; std::memcpy(&bits, &v, 4); Put32(p, bits); }
        float GetFloat(const std::uint8_t* p)
        { const auto bits = Get32(p); float v; std::memcpy(&v, &bits, 4); return v; }
        bool Nonzero(const Digest256& hash)
        { return std::any_of(hash.begin(), hash.end(), [](const std::uint8_t v) { return v != 0; }); }
        bool Valid(const Compatibility& c) { return Nonzero(c.executable) && Nonzero(c.adapter); }
        bool Valid(const LevelDescriptor& l) { return l.levelId != 0 && Nonzero(l.resources) && Nonzero(l.scenario); }
        void PutCompatibility(std::uint8_t* p, const Compatibility& c)
        { std::copy(c.executable.begin(), c.executable.end(), p); std::copy(c.adapter.begin(), c.adapter.end(), p + 32); }
        Compatibility GetCompatibility(const std::uint8_t* p)
        { Compatibility c; std::copy_n(p, 32, c.executable.begin()); std::copy_n(p + 32, 32, c.adapter.begin()); return c; }
        void PutLevel(std::uint8_t* p, const LevelDescriptor& l)
        {
            Put32(p, l.levelId); Put32(p + 4, l.entryId);
            std::copy(l.resources.begin(), l.resources.end(), p + 8);
            std::copy(l.scenario.begin(), l.scenario.end(), p + 40);
        }
        LevelDescriptor GetLevel(const std::uint8_t* p)
        {
            LevelDescriptor l; l.levelId = Get32(p); l.entryId = Get32(p + 4);
            std::copy_n(p + 8, 32, l.resources.begin()); std::copy_n(p + 40, 32, l.scenario.begin()); return l;
        }
        bool Valid(const InputIntent& i)
        {
            return i.clientTick != 0 && std::isfinite(i.magnitude) && i.magnitude >= 0 && i.magnitude <= 1 &&
                std::isfinite(i.relativeRadians) && std::abs(i.relativeRadians) <= InputIntent::MaximumRadians &&
                std::isfinite(i.referenceHeading) && std::abs(i.referenceHeading) <= InputIntent::MaximumRadians;
        }
        bool SameInputState(const InputIntent& a, const InputIntent& b)
        {
            // Signed zero carries no transition; every other field change is
            // preserved. In particular, reference heading is not discarded.
            return a.magnitude == b.magnitude && a.relativeRadians == b.relativeRadians &&
                a.referenceHeading == b.referenceHeading && a.jumpHeld == b.jumpHeld;
        }
        bool Valid(const AcceptedState& s)
        {
            if((s.entity != GameplayProtocol::HostEntity && s.entity != GameplayProtocol::GuestEntity) || s.hostTick == 0 ||
               static_cast<unsigned>(s.animation) > static_cast<unsigned>(Animation::Jump)) return false;
            for(const float v : s.position) if(!std::isfinite(v)) return false;
            for(const float v : s.velocity) if(!std::isfinite(v)) return false;
            for(const float v : s.rotation) if(!std::isfinite(v)) return false;
            // An orientation must preserve lengths, right angles and handedness.
            // Validate both row lengths and pairwise orthogonality, plus det +1.
            constexpr double tolerance = 0.01;
            for(unsigned row = 0; row < 3; ++row)
                for(unsigned other = row; other < 3; ++other)
                {
                    double dot = 0;
                    for(unsigned col = 0; col < 3; ++col) dot += double(s.rotation[row * 3 + col]) * s.rotation[other * 3 + col];
                    if(std::abs(dot - (row == other ? 1.0 : 0.0)) > tolerance) return false;
                }
            const auto& m = s.rotation;
            const double det = double(m[0]) * (double(m[4])*m[8] - double(m[5])*m[7])
                - double(m[1]) * (double(m[3])*m[8] - double(m[5])*m[6])
                + double(m[2]) * (double(m[3])*m[7] - double(m[4])*m[6]);
            return std::abs(det - 1.0) <= tolerance;
        }
        std::size_t BodySize(const MessageKind kind)
        {
            switch(kind)
            {
                case MessageKind::Hello: return CompatibilitySize + LevelSize;
                case MessageKind::Welcome: return CompatibilitySize + LevelSize + 8;
                case MessageKind::Ready: case MessageKind::Resume: return LevelSize;
                case MessageKind::Input: return 20;
                case MessageKind::State: return 72;
                case MessageKind::Remove: return 4;
            }
            return GameplayProtocol::MaximumPayload;
        }
        bool HostSends(const MessageKind kind)
        { return kind == MessageKind::Welcome || kind == MessageKind::Resume || kind == MessageKind::State || kind == MessageKind::Remove; }
    }

    bool operator==(const Compatibility& a, const Compatibility& b) noexcept
    { return a.executable == b.executable && a.adapter == b.adapter; }
    bool operator==(const LevelDescriptor& a, const LevelDescriptor& b) noexcept
    { return a.levelId == b.levelId && a.entryId == b.entryId && a.resources == b.resources && a.scenario == b.scenario; }

    struct GameplayProtocol::Header
    {
        MessageKind kind;
        std::uint64_t session;
        std::uint32_t epoch, sequence, entity;
    };

    GameplayProtocol::GameplayProtocol(Config config) : config_(std::move(config))
    {
        if((config_.role != networking::SessionRole::Host && config_.role != networking::SessionRole::Guest) ||
           !Valid(config_.compatibility) || !Valid(config_.qualifiedLevel) ||
           (config_.role == networking::SessionRole::Host ? config_.hostSession == 0 : config_.hostSession != 0))
            throw std::invalid_argument("Gameplay protocol requires exact qualified identity and a fresh host session");
        session_ = config_.hostSession;
    }
    bool GameplayProtocol::Terminal() const noexcept { return phase_ == Phase::Closed || phase_ == Phase::Failed; }
    bool GameplayProtocol::EntityAlive(const std::uint32_t entity) const noexcept
    { return entity < alive_.size() && alive_[entity]; }
    Event GameplayProtocol::MakeEvent(const EventKind kind, const std::uint32_t entity, const std::uint32_t sequence) const
    {
        Event e; e.kind = kind; e.session = session_; e.epoch = epoch_; e.entity = entity;
        e.sequence = sequence; e.level = config_.qualifiedLevel; return e;
    }
    void GameplayProtocol::Fail(const Error error) noexcept
    {
        if(Terminal()) return;
        const auto remote = config_.role == networking::SessionRole::Host ? GuestEntity : HostEntity;
        const bool remove = EntityAlive(remote);
        phase_ = Phase::Failed; error_ = error; localReady_ = peerReady_ = false;
        outbound_.clear(); events_.clear(); alive_.fill(false); lastInput_.reset();
        try
        {
            if(remove) events_.push_back(MakeEvent(EventKind::EntityRemoved, remote));
            events_.push_back(MakeEvent(EventKind::Disconnected));
        }
        catch(...) { events_.clear(); }
    }
    bool GameplayProtocol::Push(Event e)
    {
        if(events_.size() >= MaximumEvents) { Fail(Error::Backpressure); return false; }
        try { events_.push_back(std::move(e)); return true; }
        catch(const std::bad_alloc&) { Fail(Error::ResourceFailure); return false; }
    }
    bool GameplayProtocol::Emit(const MessageKind kind, const std::uint32_t entity, const void* body, const std::size_t size)
    {
        const auto k = static_cast<std::size_t>(kind);
        if(Terminal() || entity >= 3 || k == 0 || k >= sent_.size() || size != BodySize(kind) || size + HeaderSize > MaximumPayload)
            return false;
        if(outbound_.size() >= MaximumOutbound) { Fail(Error::Backpressure); return false; }
        if(sent_[k][entity] == std::numeric_limits<std::uint32_t>::max()) { Fail(Error::SequenceExhausted); return false; }
        try
        {
            networking::SessionMessage message; message.classId = ClassId; message.key = static_cast<std::uint32_t>(kind);
            message.payload.resize(HeaderSize + size);
            auto* p = message.payload.data(); Put16(p, Version); p[2] = static_cast<std::uint8_t>(kind); p[3] = 0;
            Put64(p + 4, kind == MessageKind::Hello ? 0 : session_);
            Put32(p + 12, kind == MessageKind::Hello ? 0 : epoch_);
            Put32(p + 16, sent_[k][entity] + 1); Put32(p + 20, entity);
            std::memcpy(p + HeaderSize, body, size);
            outbound_.push_back(std::move(message)); ++sent_[k][entity]; return true;
        }
        catch(const std::bad_alloc&) { Fail(Error::ResourceFailure); return false; }
    }
    bool GameplayProtocol::Begin(const std::uint64_t connectionId)
    {
        if(phase_ != Phase::Idle || connectionId == 0) return false;
        connection_ = connectionId;
        if(config_.role == networking::SessionRole::Host) { phase_ = Phase::AwaitingHello; return true; }
        phase_ = Phase::AwaitingAssignment;
        std::array<std::uint8_t, CompatibilitySize + LevelSize> body{};
        PutCompatibility(body.data(), config_.compatibility); PutLevel(body.data() + CompatibilitySize, config_.qualifiedLevel);
        return Emit(MessageKind::Hello, 0, body.data(), body.size());
    }
    bool GameplayProtocol::HostAssignLevel(const std::uint32_t epoch, const LevelDescriptor& level)
    {
        if(config_.role != networking::SessionRole::Host || Terminal() || phase_ == Phase::Idle || epoch == 0 || !(level == config_.qualifiedLevel)) return false;
        if(assigned_) return epoch == epoch_;
        if(phase_ != Phase::AwaitingHello && phase_ != Phase::WaitingForHostLevel) return false;
        epoch_ = epoch; assigned_ = localReady_ = true;
        if(!Push(MakeEvent(EventKind::LevelAssigned))) return false;
        return !helloReceived_ || SendWelcome();
    }
    bool GameplayProtocol::SendWelcome()
    {
        if(!assigned_ || !localReady_ || !helloReceived_ || welcomeSent_) return false;
        std::array<std::uint8_t, CompatibilitySize + LevelSize + 8> body{};
        PutCompatibility(body.data(), config_.compatibility); PutLevel(body.data() + CompatibilitySize, config_.qualifiedLevel);
        Put32(body.data() + CompatibilitySize + LevelSize, HostEntity);
        Put32(body.data() + CompatibilitySize + LevelSize + 4, GuestEntity);
        if(!Emit(MessageKind::Welcome, 0, body.data(), body.size())) return false;
        welcomeSent_ = true; alive_[HostEntity] = alive_[GuestEntity] = true; phase_ = Phase::Loading; return true;
    }
    bool GameplayProtocol::NotifyLocalReady(const LevelDescriptor& level)
    {
        if(!assigned_ || Terminal() || !(level == config_.qualifiedLevel)) return false;
        if(localReady_) return true;
        if(config_.role != networking::SessionRole::Guest || phase_ != Phase::Loading) return false;
        std::array<std::uint8_t, LevelSize> body{}; PutLevel(body.data(), level);
        if(!Emit(MessageKind::Ready, GuestEntity, body.data(), body.size())) return false;
        localReady_ = true; return true;
    }
    bool GameplayProtocol::SendResume()
    {
        if(!localReady_ || !peerReady_ || phase_ != Phase::Loading) return false;
        std::array<std::uint8_t, LevelSize> body{}; PutLevel(body.data(), config_.qualifiedLevel);
        if(!Emit(MessageKind::Resume, 0, body.data(), body.size())) return false;
        phase_ = Phase::Running; return Push(MakeEvent(EventKind::Started));
    }
    bool GameplayProtocol::SubmitInput(const InputIntent& input)
    {
        if(config_.role != networking::SessionRole::Guest || phase_ != Phase::Running || !EntityAlive(GuestEntity) ||
           !Valid(input) || input.clientTick <= lastInputTick_) return false;
        if(lastInput_ && input.clientTick - lastInputTick_ != 1)
        { Fail(Error::InputDiscontinuity); return false; }
        if(lastInput_ && SameInputState(input, *lastInput_))
        {
            // Do not replace the queued transition's first observation tick.
            lastInputTick_ = input.clientTick; return true;
        }
        std::array<std::uint8_t, 20> body{}; Put32(body.data(), input.clientTick);
        PutFloat(body.data() + 4, input.magnitude); PutFloat(body.data() + 8, input.relativeRadians);
        PutFloat(body.data() + 12, input.referenceHeading); Put16(body.data() + 16, static_cast<std::uint16_t>(input.jumpHeld ? 1 : 0));
        if(!Emit(MessageKind::Input, GuestEntity, body.data(), body.size())) return false;
        lastInput_ = input; lastInputTick_ = input.clientTick;
        lastGuestInput_ = sent_[static_cast<std::size_t>(MessageKind::Input)][GuestEntity]; return true;
    }
    bool GameplayProtocol::PublishAcceptedState(const AcceptedState& state)
    {
        if(config_.role != networking::SessionRole::Host || phase_ != Phase::Running || !Valid(state) || !EntityAlive(state.entity) ||
           state.hostTick <= stateTicks_[state.entity] || state.acknowledgedInput < stateAcknowledgements_[state.entity] ||
           (state.entity == HostEntity ? state.acknowledgedInput != 0 : state.acknowledgedInput > lastGuestInput_)) return false;
        std::array<std::uint8_t, 72> body{}; Put32(body.data(), state.hostTick); Put32(body.data() + 4, state.acknowledgedInput);
        for(unsigned i = 0; i < 3; ++i) PutFloat(body.data() + 8 + i * 4, state.position[i]);
        for(unsigned i = 0; i < 3; ++i) PutFloat(body.data() + 20 + i * 4, state.velocity[i]);
        for(unsigned i = 0; i < 9; ++i) PutFloat(body.data() + 32 + i * 4, state.rotation[i]);
        Put16(body.data() + 68, static_cast<std::uint16_t>(state.animation));
        if(!Emit(MessageKind::State, state.entity, body.data(), body.size())) return false;
        stateTicks_[state.entity] = state.hostTick; stateAcknowledgements_[state.entity] = state.acknowledgedInput; return true;
    }
    bool GameplayProtocol::RemoveEntity(const std::uint32_t entity, const std::uint16_t reason)
    {
        if(config_.role != networking::SessionRole::Host || phase_ != Phase::Running || entity < 1 || entity > 2) return false;
        if(!EntityAlive(entity)) return true;
        std::array<std::uint8_t, 4> body{}; Put16(body.data(), reason);
        if(!Emit(MessageKind::Remove, entity, body.data(), body.size())) return false;
        alive_[entity] = false; auto event = MakeEvent(EventKind::EntityRemoved, entity); event.removalReason = reason; return Push(std::move(event));
    }
    ReceiveResult GameplayProtocol::Receive(const std::uint64_t connectionId, const networking::SessionMessage& message)
    {
        if(connectionId == 0 || connectionId != connection_) return ReceiveResult::WrongConnection;
        if(Terminal() || phase_ == Phase::Idle) return ReceiveResult::Rejected;
        if(message.classId != ClassId || message.payload.size() < HeaderSize || message.payload.size() > MaximumPayload)
        { Fail(Error::InvalidMessage); return ReceiveResult::Rejected; }
        const auto* p = message.payload.data();
        const Header h{static_cast<MessageKind>(p[2]), Get64(p + 4), Get32(p + 12), Get32(p + 16), Get32(p + 20)};
        const auto k = static_cast<std::size_t>(h.kind);
        if(Get16(p) != Version || p[3] != 0 || k < 1 || k > 7 || message.key != k || h.entity > 2 || h.sequence == 0 ||
           message.payload.size() != HeaderSize + BodySize(h.kind))
        { Fail(Error::InvalidMessage); return ReceiveResult::Rejected; }
        if(HostSends(h.kind) == (config_.role == networking::SessionRole::Host))
        { Fail(Error::WrongRole); return ReceiveResult::Rejected; }
        if(h.kind == MessageKind::Hello)
        {
            if(h.session != 0 || h.epoch != 0 || h.entity != 0) { Fail(Error::InvalidMessage); return ReceiveResult::Rejected; }
        }
        else if(h.kind == MessageKind::Welcome)
        {
            if(h.session == 0 || h.epoch == 0 || h.entity != 0) { Fail(Error::InvalidMessage); return ReceiveResult::Rejected; }
            if(assigned_ && (h.session != session_ || h.epoch != epoch_)) return ReceiveResult::IgnoredStale;
        }
        else
        {
            if(!assigned_) { Fail(Error::WrongPhase); return ReceiveResult::Rejected; }
            if(h.session != session_ || h.epoch < epoch_) return ReceiveResult::IgnoredStale;
            if(h.epoch != epoch_) { Fail(Error::InvalidEpoch); return ReceiveResult::Rejected; }
        }
        if(h.sequence <= received_[k][h.entity]) return ReceiveResult::IgnoredStale;
        try
        {
            const auto result = Handle(h, p + HeaderSize, message.payload.size() - HeaderSize);
            if(result == ReceiveResult::Accepted) received_[k][h.entity] = h.sequence;
            return result;
        }
        catch(const std::bad_alloc&) { Fail(Error::ResourceFailure); return ReceiveResult::Rejected; }
    }
    ReceiveResult GameplayProtocol::Handle(const Header& h, const std::uint8_t* body, const std::size_t)
    {
        switch(h.kind)
        {
            case MessageKind::Hello:
                if(phase_ != Phase::AwaitingHello) { Fail(Error::WrongPhase); break; }
                if(!(GetCompatibility(body) == config_.compatibility) || !(GetLevel(body + CompatibilitySize) == config_.qualifiedLevel))
                { Fail(Error::Incompatible); break; }
                helloReceived_ = true; phase_ = Phase::WaitingForHostLevel;
                if(!assigned_ || SendWelcome()) return ReceiveResult::Accepted;
                break;
            case MessageKind::Welcome:
                if(phase_ != Phase::AwaitingAssignment) { Fail(Error::WrongPhase); break; }
                if(!(GetCompatibility(body) == config_.compatibility) || !(GetLevel(body + CompatibilitySize) == config_.qualifiedLevel) ||
                   Get32(body + CompatibilitySize + LevelSize) != HostEntity || Get32(body + CompatibilitySize + LevelSize + 4) != GuestEntity)
                { Fail(Error::Incompatible); break; }
                session_ = h.session; epoch_ = h.epoch; assigned_ = true; phase_ = Phase::Loading;
                alive_[HostEntity] = alive_[GuestEntity] = true;
                if(Push(MakeEvent(EventKind::LevelAssigned, 0, h.sequence))) return ReceiveResult::Accepted;
                break;
            case MessageKind::Ready:
                if(phase_ != Phase::Loading || !welcomeSent_ || h.entity != GuestEntity) { Fail(Error::WrongPhase); break; }
                if(!(GetLevel(body) == config_.qualifiedLevel)) { Fail(Error::Incompatible); break; }
                peerReady_ = true;
                if(SendResume()) return ReceiveResult::Accepted;
                break;
            case MessageKind::Resume:
                if(phase_ != Phase::Loading || !localReady_ || h.entity != 0) { Fail(Error::WrongPhase); break; }
                if(!(GetLevel(body) == config_.qualifiedLevel)) { Fail(Error::Incompatible); break; }
                peerReady_ = true; phase_ = Phase::Running;
                if(Push(MakeEvent(EventKind::Started, 0, h.sequence))) return ReceiveResult::Accepted;
                break;
            case MessageKind::Input:
            {
                if(phase_ != Phase::Running || h.entity != GuestEntity) { Fail(Error::WrongPhase); break; }
                if(!EntityAlive(GuestEntity)) return ReceiveResult::IgnoredStale;
                InputIntent input; input.clientTick = Get32(body);
                input.magnitude = GetFloat(body + 4); input.relativeRadians = GetFloat(body + 8);
                input.referenceHeading = GetFloat(body + 12); const auto flags = Get16(body + 16);
                input.jumpHeld = (flags & 1) != 0;
                if(!Valid(input) || (flags & ~std::uint16_t(1)) != 0 || Get16(body + 18) != 0)
                { Fail(Error::InvalidMessage); break; }
                // Reliable ordered carrier plus contiguous transition sequence
                // detects a dropped/coalesced press or release. Packet replay
                // was discarded before Handle; a *fresh* sequence cannot move
                // the producer's native observation clock backwards.
                if(h.sequence != lastGuestInput_ + 1 || input.clientTick <= lastInputTick_)
                { Fail(Error::InputDiscontinuity); break; }
                if(lastInput_ && SameInputState(input, *lastInput_)) { Fail(Error::InvalidMessage); break; }
                auto e = MakeEvent(EventKind::GuestInput, GuestEntity, h.sequence); e.input = input;
                if(!Push(std::move(e))) break;
                lastInput_ = input; lastGuestInput_ = h.sequence; lastInputTick_ = input.clientTick; return ReceiveResult::Accepted;
            }
            case MessageKind::State:
            {
                if(phase_ != Phase::Running || h.entity < 1) { Fail(Error::WrongPhase); break; }
                if(!EntityAlive(h.entity)) return ReceiveResult::IgnoredStale;
                AcceptedState state; state.entity = h.entity; state.hostTick = Get32(body); state.acknowledgedInput = Get32(body + 4);
                for(unsigned i = 0; i < 3; ++i) state.position[i] = GetFloat(body + 8 + i * 4);
                for(unsigned i = 0; i < 3; ++i) state.velocity[i] = GetFloat(body + 20 + i * 4);
                for(unsigned i = 0; i < 9; ++i) state.rotation[i] = GetFloat(body + 32 + i * 4);
                state.animation = static_cast<Animation>(Get16(body + 68));
                if(!Valid(state) || Get16(body + 70) != 0 ||
                   (h.entity == HostEntity ? state.acknowledgedInput != 0 : state.acknowledgedInput > lastGuestInput_))
                { Fail(Error::InvalidMessage); break; }
                if(state.hostTick <= stateTicks_[h.entity] || state.acknowledgedInput < stateAcknowledgements_[h.entity])
                    return ReceiveResult::IgnoredStale;
                auto e = MakeEvent(EventKind::AcceptedState, h.entity, h.sequence); e.state = state;
                if(!Push(std::move(e))) break;
                stateTicks_[h.entity] = state.hostTick; stateAcknowledgements_[h.entity] = state.acknowledgedInput; return ReceiveResult::Accepted;
            }
            case MessageKind::Remove:
                if(phase_ != Phase::Running || h.entity < 1 || Get16(body + 2) != 0)
                { Fail(Error::InvalidMessage); break; }
                if(!EntityAlive(h.entity)) return ReceiveResult::IgnoredStale;
                alive_[h.entity] = false;
                { auto e = MakeEvent(EventKind::EntityRemoved, h.entity, h.sequence); e.removalReason = Get16(body);
                  if(Push(std::move(e))) return ReceiveResult::Accepted; }
                break;
        }
        return ReceiveResult::Rejected;
    }
    const networking::SessionMessage* GameplayProtocol::PeekOutbound() const noexcept
    { return outbound_.empty() ? nullptr : &outbound_.front(); }
    bool GameplayProtocol::CommitOutbound() noexcept
    { if(outbound_.empty()) return false; outbound_.pop_front(); return true; }
    std::optional<networking::SessionMessage> GameplayProtocol::TakeOutbound()
    { if(outbound_.empty()) return std::nullopt; auto message = std::move(outbound_.front()); outbound_.pop_front(); return message; }
    std::optional<Event> GameplayProtocol::TakeEvent()
    { if(events_.empty()) return std::nullopt; auto event = std::move(events_.front()); events_.pop_front(); return event; }
    void GameplayProtocol::Close() noexcept
    {
        if(Terminal()) return;
        const auto remote = config_.role == networking::SessionRole::Host ? GuestEntity : HostEntity;
        const bool remove = EntityAlive(remote);
        phase_ = Phase::Closed; localReady_ = peerReady_ = false; outbound_.clear(); events_.clear(); alive_.fill(false); lastInput_.reset();
        try
        {
            if(remove) events_.push_back(MakeEvent(EventKind::EntityRemoved, remote));
            events_.push_back(MakeEvent(EventKind::Disconnected));
        }
        catch(...) { events_.clear(); }
    }
}
