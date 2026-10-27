#include "../src/Gameplay/GameplayProtocol.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace winxcoop::gameplay;
using winxcoop::networking::SessionMessage;
using winxcoop::networking::SessionRole;
namespace
{
    unsigned checks = 0;
    void Check(const bool condition, const char* text)
    { ++checks; if(!condition) throw std::runtime_error(text); }
    Config Configuration(const SessionRole role, const std::uint64_t session = 0x1020304050607080ull)
    {
        Config c; c.role = role; c.compatibility.executable.fill(0x11); c.compatibility.adapter.fill(0x22);
        c.qualifiedLevel.levelId = 28; c.qualifiedLevel.entryId = 2;
        c.qualifiedLevel.resources.fill(0x33); c.qualifiedLevel.scenario.fill(0x44);
        c.hostSession = role == SessionRole::Host ? session : 0; return c;
    }
    void Set32(std::vector<std::uint8_t>& p, const std::size_t at, const std::uint32_t v)
    { for(unsigned n = 0; n < 4; ++n) p.at(at + n) = static_cast<std::uint8_t>(v >> (n * 8)); }
    std::vector<std::uint8_t> Bytes(const char* text)
    {
        std::string s; for(const char* p = text; *p; ++p) if(!std::isspace(static_cast<unsigned char>(*p))) s += *p;
        Check(s.size() % 2 == 0, "complete independent hex literal"); std::vector<std::uint8_t> out;
        for(std::size_t i = 0; i < s.size(); i += 2) out.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2), nullptr, 16)));
        return out;
    }
    SessionMessage Next(GameplayProtocol& p)
    { auto m = p.TakeOutbound(); Check(m.has_value(), "one expected owned application message"); return std::move(*m); }
    void Drain(GameplayProtocol& p) { while(p.TakeEvent()) {} }
    void Accepted(GameplayProtocol& p, const SessionMessage& m, const std::uint64_t connection = 11)
    { Check(p.Receive(connection, m) == ReceiveResult::Accepted, "application message accepted for exactly its connection"); }
    Event NextEvent(GameplayProtocol& p, const EventKind kind)
    { auto e = p.TakeEvent(); Check(e.has_value() && e->kind == kind, "expected owned gameplay event"); return *e; }
    struct Pair
    {
        Config h = Configuration(SessionRole::Host), g = Configuration(SessionRole::Guest);
        GameplayProtocol host{h}, guest{g};
        void Begin()
        { Check(host.Begin(11) && guest.Begin(11), "both endpoints begin once on an application carrier"); }
        void Welcome(const std::uint32_t epoch = 7)
        {
            Begin(); Accepted(host, Next(guest));
            Check(host.HostAssignLevel(epoch, h.qualifiedLevel), "actual ready host assigns its qualified scene");
            Accepted(guest, Next(host));
        }
        void Run(const std::uint32_t epoch = 7)
        {
            Welcome(epoch); Check(guest.NotifyLocalReady(g.qualifiedLevel), "guest reports exact actual scene readiness");
            Accepted(host, Next(guest)); Accepted(guest, Next(host));
            Check(host.CurrentPhase() == Phase::Running && guest.CurrentPhase() == Phase::Running, "only host Resume starts both gameplay endpoints");
            Drain(host); Drain(guest);
        }
    };
    AcceptedState State(const std::uint32_t entity, const std::uint32_t tick)
    { AcceptedState s; s.entity = entity; s.hostTick = tick; s.position = {{1,2,3}}; s.velocity = {{4,0,0}}; s.animation = Animation::Walk; return s; }
    InputIntent Input(const std::uint32_t tick)
    {
        InputIntent i; i.clientTick = tick; i.magnitude = 1;
        i.relativeRadians = 1.57079637050628662109375f; i.referenceHeading = -1;
        i.jumpHeld = (tick & 1) == 0; return i;
    }

    void GoldenAndWaiting()
    {
        Pair p; p.Begin(); auto hello = Next(p.guest);
        const auto expected = Bytes(
            "02000100 0000000000000000 00000000 01000000 00000000 "
            "1111111111111111111111111111111111111111111111111111111111111111 "
            "2222222222222222222222222222222222222222222222222222222222222222 "
            "1c000000 02000000 "
            "3333333333333333333333333333333333333333333333333333333333333333 "
            "4444444444444444444444444444444444444444444444444444444444444444");
        Check(hello.classId == 0x31504357 && hello.key == 1 && hello.payload == expected && expected.size() == 160,
            "independent golden Hello uses application payload, not the original Packet header");
        Accepted(p.host, hello);
        Check(p.host.CurrentPhase() == Phase::WaitingForHostLevel && p.guest.CurrentPhase() == Phase::AwaitingAssignment && !p.host.TakeOutbound(),
            "compatible client waits while host has not started a ready campaign");
        Check(!p.guest.HostAssignLevel(7,p.g.qualifiedLevel) && !p.guest.SubmitInput(Input(1)) && !p.guest.NotifyLocalReady(p.g.qualifiedLevel),
            "guest cannot choose a scene, move, or become ready before host assignment");
        Check(p.host.HostAssignLevel(7,p.h.qualifiedLevel), "host alone publishes actual qualified level");
        auto welcome = Next(p.host); auto expectedWelcome = expected;
        expectedWelcome[2] = 2;
        const auto session = Bytes("8070605040302010"); std::copy(session.begin(), session.end(), expectedWelcome.begin() + 4);
        Set32(expectedWelcome, 12, 7); expectedWelcome.insert(expectedWelcome.end(), {1,0,0,0,2,0,0,0});
        Check(welcome.key == 2 && welcome.payload == expectedWelcome && welcome.payload.size() == 168,
            "golden Welcome assigns exact session, epoch, two entities, and all fingerprints");
        Accepted(p.guest, welcome);
        Check(p.host.CurrentPhase() == Phase::Loading && p.guest.CurrentPhase() == Phase::Loading && !p.guest.SubmitInput(Input(2)),
            "assignment does not start simulation before guest scene ready");
        auto wrong = p.g.qualifiedLevel; ++wrong.entryId;
        Check(!p.guest.NotifyLocalReady(wrong) && !p.guest.TakeOutbound(), "same level but different entry is not scene readiness");
        Check(p.guest.NotifyLocalReady(p.g.qualifiedLevel) && p.guest.NotifyLocalReady(p.g.qualifiedLevel), "readiness report is idempotent");
        auto ready = Next(p.guest); Check(ready.payload.size() == 96 && !p.guest.TakeOutbound(), "one exact Ready body despite repeated local report");
        Accepted(p.host, ready); auto resume = Next(p.host);
        Check(resume.payload.size() == 96 && p.host.CurrentPhase() == Phase::Running && p.guest.CurrentPhase() == Phase::Loading,
            "host explicitly emits Resume after both actual ready reports");
        Accepted(p.guest, resume);
        Check(p.guest.CurrentPhase() == Phase::Running && p.guest.SessionId() == p.h.hostSession && p.guest.LevelEpoch() == 7,
            "guest adopts only host generation and permission");
        Check(p.guest.Receive(11, resume) == ReceiveResult::IgnoredStale && p.host.Receive(11, ready) == ReceiveResult::IgnoredStale,
            "duplicate control packets do not repeat readiness or start events");
        Check(NextEvent(p.guest, EventKind::LevelAssigned).level == p.g.qualifiedLevel, "assignment event owns full exact descriptor");
        (void)NextEvent(p.guest, EventKind::Started); Check(!p.guest.TakeEvent(), "one guest assignment and start only");
    }
    void CompatibilityFailures()
    {
        for(unsigned field = 0; field < 6; ++field)
        {
            auto h = Configuration(SessionRole::Host), g = Configuration(SessionRole::Guest);
            if(field == 0) ++g.compatibility.executable[0];
            if(field == 1) ++g.compatibility.adapter[31];
            if(field == 2) ++g.qualifiedLevel.levelId;
            if(field == 3) ++g.qualifiedLevel.entryId;
            if(field == 4) ++g.qualifiedLevel.resources[12];
            if(field == 5) ++g.qualifiedLevel.scenario[9];
            GameplayProtocol host(h), guest(g); Check(host.Begin(11) && guest.Begin(11), "mismatch endpoints begin transport only");
            Check(host.Receive(11, Next(guest)) == ReceiveResult::Rejected && host.LastError() == Error::Incompatible && host.CurrentPhase() == Phase::Failed,
                "EXE, adapter, level, entry, resource, and scenario mismatches each fail before assignment");
            Check(!host.TakeOutbound(), "incompatible guest never gets a scene assignment");
        }
        Pair p; p.Welcome(); Check(p.guest.NotifyLocalReady(p.g.qualifiedLevel), "prepare forged Ready test");
        auto ready = Next(p.guest); ++ready.payload[24 + 40];
        Check(p.host.Receive(11,ready) == ReceiveResult::Rejected && p.host.LastError() == Error::Incompatible && !p.host.TakeOutbound(),
            "Ready must still prove the exact scenario; no Resume after a changed fingerprint");
    }
    void InputAndAuthority()
    {
        Pair p; p.Run(); Check(p.guest.SubmitInput(Input(42)), "guest sends native polar intent after Resume");
        auto input = Next(p.guest);
        Check(input.payload.size() == 44 && std::vector<std::uint8_t>(input.payload.begin() + 24,input.payload.end()) ==
            Bytes("2a000000 0000803f db0fc93f 000080bf 0100 0000"),
            "v2 independent golden input contains tick, magnitude, relative/reference radians and jump-held only");
        Accepted(p.host,input); auto e = NextEvent(p.host,EventKind::GuestInput);
        Check(e.entity == GameplayProtocol::GuestEntity && e.sequence == 1 && e.input.magnitude == 1 &&
            e.input.relativeRadians == Input(42).relativeRadians && e.input.referenceHeading == -1 && e.input.jumpHeld && e.input.clientTick == 42,
            "host receives guest intent with connection generation and sequence");
        Check(!p.host.TakeOutbound(), "input receipt does not synthesize accepted movement");
        Check(!p.host.SubmitInput(Input(43)) && !p.guest.PublishAcceptedState(State(1,1)), "local API enforces sender authority");
        auto own = State(1,90), remote = State(2,90); remote.acknowledgedInput = e.sequence;
        Check(p.host.PublishAcceptedState(own) && p.host.PublishAcceptedState(remote), "trusted backend supplies separate host and guest accepted states");
        auto first = Next(p.host), second = Next(p.host);
        Check(first.payload.size() == 96 && second.payload.size() == 96 && first.payload[16] == 1 && second.payload[16] == 1,
            "state sequence is independent for both assigned entities");
        Accepted(p.guest, first); Accepted(p.guest, second);
        Check(NextEvent(p.guest,EventKind::AcceptedState).state.position == own.position, "host actor accepted pose delivered");
        auto acceptedGuest = NextEvent(p.guest,EventKind::AcceptedState);
        Check(acceptedGuest.state.acknowledgedInput == 1 && acceptedGuest.state.rotation == remote.rotation,
            "guest correction carries input acknowledgement and full 3x3 orientation");
        Check(p.guest.Receive(11,second) == ReceiveResult::IgnoredStale && p.host.Receive(11,input) == ReceiveResult::IgnoredStale && !p.guest.TakeEvent(),
            "duplicate input and state cannot apply twice");
        auto futureAck = State(2,91); futureAck.acknowledgedInput = 2;
        Check(!p.host.PublishAcceptedState(futureAck), "host cannot acknowledge an input it has not received");
        auto decreasing = State(1,89); Check(!p.host.PublishAcceptedState(decreasing), "trusted API still rejects old host ticks");
        Check(p.host.Receive(11, first) == ReceiveResult::Rejected && p.host.LastError() == Error::WrongRole,
            "guest transport cannot smuggle a state packet into host authority");
        Check(!p.host.Begin(11) && !p.host.HostAssignLevel(8,p.h.qualifiedLevel), "failed authority connection is terminal");
    }
    void InvalidStates()
    {
        for(unsigned defect = 0; defect < 7; ++defect)
        {
            Pair p; p.Run(); Check(p.host.PublishAcceptedState(State(1,10)), "prepare independently corrupted accepted state");
            auto m = Next(p.host);
            if(defect == 0) Set32(m.payload,32,0x7fc00000); // position NaN
            if(defect == 1) Set32(m.payload,44,0x7f800000); // velocity +Inf
            if(defect == 2) Set32(m.payload,56,0x40000000); // matrix row length 2
            if(defect == 3) Set32(m.payload,56,0xbf800000); // reflection, det -1
            if(defect == 4) Set32(m.payload,60,0x3f800000); // skew
            if(defect == 5) m.payload[92] = 3; // unknown animation
            if(defect == 6) m.payload[94] = 1; // reserved
            Check(p.guest.Receive(11,m) == ReceiveResult::Rejected && p.guest.LastError() == Error::InvalidMessage,
                "nonfinite, scaled/reflected/skew orientation, unknown animation and reserved bits are rejected before application");
            while(auto e = p.guest.TakeEvent()) Check(e->kind != EventKind::AcceptedState, "invalid state never reaches adapter event queue");
        }
        Pair p; p.Run(); auto s = State(1,1); s.position[0] = std::numeric_limits<float>::infinity();
        Check(!p.host.PublishAcceptedState(s) && !p.host.TakeOutbound() && p.host.CurrentPhase() == Phase::Running,
            "bad local backend state is refused without emitting traffic");
        auto i = Input(1); i.magnitude = -0.01f; Check(!p.guest.SubmitInput(i), "negative polar magnitude rejected");
        i = Input(1); i.relativeRadians = 4; Check(!p.guest.SubmitInput(i), "out-of-range native direction rejected without normalization");
    }
    void GenerationsAndRemoval()
    {
        Pair old; old.Run(); Check(old.host.PublishAcceptedState(State(1,100)), "old scene produces owned state");
        const auto message = Next(old.host);
        Check(old.guest.Receive(12,message) == ReceiveResult::WrongConnection && old.guest.CurrentPhase() == Phase::Running,
            "another socket's message cannot mutate this instance or consume its sequence");
        auto alienSession = message; alienSession.payload[4] ^= 1;
        Check(old.guest.Receive(11,alienSession) == ReceiveResult::IgnoredStale, "old or foreign session discarded before pose decode");
        Accepted(old.guest,message); Drain(old.guest);
        Pair newer; newer.Run(8);
        Check(newer.guest.Receive(11,message) == ReceiveResult::IgnoredStale && !newer.guest.TakeEvent(), "previous epoch ignored even if transport identifier is recycled");
        auto future = message; Set32(future.payload,12,9);
        Check(newer.guest.Receive(11,future) == ReceiveResult::Rejected && newer.guest.LastError() == Error::InvalidEpoch,
            "future level generation cannot be introduced by a state packet");
        Check(old.host.RemoveEntity(2,7) && old.host.RemoveEntity(2,7), "host removal API idempotent");
        auto remove = Next(old.host); Check(remove.payload.size() == 28 && !old.host.TakeOutbound(), "only one removal message");
        Accepted(old.guest, remove); auto removal = NextEvent(old.guest,EventKind::EntityRemoved);
        Check(removal.entity == 2 && removal.removalReason == 7 && !old.guest.EntityAlive(2), "remove invalidates exact assigned entity");
        Check(old.guest.Receive(11,remove) == ReceiveResult::IgnoredStale && !old.guest.TakeEvent(), "duplicate removal has no additional event");
        Set32(remove.payload,16,2);
        Check(old.guest.Receive(11,remove) == ReceiveResult::IgnoredStale && !old.guest.TakeEvent(), "repeated remove with a fresh sequence still cannot delete twice");
        Check(!old.guest.SubmitInput(Input(2)) && !old.host.PublishAcceptedState(State(2,101)), "removed entity cannot send input or be resurrected by snapshots");
        old.guest.Close(); old.guest.Close();
        Check(NextEvent(old.guest,EventKind::EntityRemoved).entity == 1, "disconnect requests removal of only remaining remote host actor");
        (void)NextEvent(old.guest,EventKind::Disconnected);
        Check(!old.guest.TakeEvent() && !old.guest.Begin(11) && old.guest.Receive(11,message) == ReceiveResult::Rejected,
            "close is terminal and idempotent, with no stale queued poses");
        auto freshConfig = Configuration(SessionRole::Guest); GameplayProtocol fresh(freshConfig);
        Check(fresh.Begin(12) && fresh.SessionId() == 0 && fresh.Receive(11,message) == ReceiveResult::WrongConnection,
            "fresh guest instance starts without inherited world handles or authority");
    }
    void MalformedAndBarrier()
    {
        for(unsigned defect = 0; defect < 8; ++defect)
        {
            Pair p; p.Begin(); auto m = Next(p.guest);
            if(defect == 0) ++m.classId;
            if(defect == 1) ++m.key;
            if(defect == 2) ++m.payload[0];
            if(defect == 3) m.payload[3] = 1;
            if(defect == 4) m.payload.pop_back();
            if(defect == 5) m.payload.resize(257);
            if(defect == 6) Set32(m.payload,16,0);
            if(defect == 7) Set32(m.payload,20,3);
            Check(p.host.Receive(11,m) == ReceiveResult::Rejected && p.host.LastError() == Error::InvalidMessage,
                "malformed class/key/version/flags/size/sequence/entity rejected before loading");
        }
        Pair p; p.Welcome(); auto fabricatedResume = NextEvent(p.guest,EventKind::LevelAssigned);
        (void)fabricatedResume;
        SessionMessage resume; resume.classId = GameplayProtocol::ClassId; resume.key = 4;
        resume.payload = Bytes("02000400 8070605040302010 07000000 01000000 00000000");
        resume.payload.resize(96); // exact framing but no valid readiness
        Check(p.guest.Receive(11,resume) == ReceiveResult::Rejected && p.guest.LastError() == Error::WrongPhase,
            "host-shaped Resume before actual local ready cannot unlock input");
    }
    void BoundedQueuesAndFreshness()
    {
        Pair p; p.Run();
        for(std::uint32_t tick = 1; tick <= GameplayProtocol::MaximumOutbound; ++tick)
            Check(p.guest.SubmitInput(Input(tick)), "bounded unsent input accepted until exact queue limit");
        auto same = Input(8); same.clientTick = 9;
        Check(p.guest.SubmitInput(same) && p.guest.OutboundCount() == GameplayProtocol::MaximumOutbound && p.guest.LastGuestInputSequence() == 8,
            "identical observation at a full queue is elided without replacing or consuming a transition sequence");
        auto changed = Input(9); changed.clientTick = 10;
        Check(!p.guest.SubmitInput(changed) && p.guest.LastError() == Error::Backpressure && p.guest.OutboundCount() == 0,
            "outbound overflow terminates and discards unsent intents instead of unbounded allocation");
        Pair q; q.Run();
        for(std::uint32_t tick = 1; tick <= GameplayProtocol::MaximumEvents; ++tick)
        { Check(q.guest.SubmitInput(Input(tick)), "prepare bounded input event"); Accepted(q.host,Next(q.guest)); }
        Check(q.host.EventCount() == GameplayProtocol::MaximumEvents && q.guest.SubmitInput(Input(17)), "input event queue reaches exact bound");
        Check(q.host.Receive(11,Next(q.guest)) == ReceiveResult::Rejected && q.host.LastError() == Error::Backpressure && q.host.EventCount() <= 2,
            "stalled adapter cannot accumulate unlimited intent or keep stale input after failure");
        Pair r; r.Run(); Check(r.guest.SubmitInput(Input(50)), "first monotonic client tick"); auto input = Next(r.guest); Accepted(r.host,input); Drain(r.host);
        Check(!r.guest.SubmitInput(Input(50)), "local producer cannot repeat or move its simulation clock backwards");
        Set32(input.payload,16,2); Set32(input.payload,24,49);
        Check(r.host.Receive(11,input) == ReceiveResult::Rejected && r.host.LastError() == Error::InputDiscontinuity,
            "fresh input event with a backwards observation tick fails closed instead of losing a temporal boundary");
        Pair s; s.Run(); Check(s.host.PublishAcceptedState(State(1,1)), "prepare state sequence exhaustion boundary");
        auto state = Next(s.host); Set32(state.payload,16,std::numeric_limits<std::uint32_t>::max());
        Accepted(s.guest,state); Drain(s.guest); Set32(state.payload,16,1); Set32(state.payload,24,2);
        Check(s.guest.Receive(11,state) == ReceiveResult::IgnoredStale && !s.guest.TakeEvent(), "nonwrapping state sequence never creates a new generation");
    }

    void PolarInputValidation()
    {
        // Structural numeric acceptance is not native analog qualification.
        Pair p; p.Run(); InputIntent analog; analog.clientTick = 1; analog.magnitude = 0.25f;
        analog.relativeRadians = -InputIntent::MaximumRadians; analog.referenceHeading = InputIntent::MaximumRadians;
        Check(p.guest.SubmitInput(analog), "finite polar wire envelope accepts both angle endpoints and bounded magnitude");
        Accepted(p.host,Next(p.guest)); auto event = NextEvent(p.host,EventKind::GuestInput);
        Check(event.input.magnitude == 0.25f && event.input.referenceHeading == InputIntent::MaximumRadians,
            "codec preserves bounded values without simulating, remapping axes or qualifying an analog backend");
        for(unsigned defect = 0; defect < 11; ++defect)
        {
            Pair q; q.Run(); Check(q.guest.SubmitInput(Input(2)), "prepare a polar event for independent corruption");
            auto message = Next(q.guest);
            if(defect == 0) Set32(message.payload,28,0xbf800000); // magnitude -1
            if(defect == 1) Set32(message.payload,28,0x3f800001); // magnitude >1
            if(defect == 2) Set32(message.payload,28,0x7fc00000); // magnitude NaN
            if(defect == 3) Set32(message.payload,32,0x7f800000); // direction Inf
            if(defect == 4) Set32(message.payload,32,0x40490fdc); // direction one float above pi
            if(defect == 5) Set32(message.payload,36,0xc0490fdc); // reference below -pi
            if(defect == 6) Set32(message.payload,36,0xffc00000); // reference NaN
            if(defect == 7) message.payload[40] = 2; // unknown held flags
            if(defect == 8) message.payload[42] = 1; // reserved
            if(defect == 9) Set32(message.payload,24,0); // zero observation tick
            if(defect == 10) message.payload.resize(36); // old axis body
            Check(q.host.Receive(11,message) == ReceiveResult::Rejected && q.host.LastError() == Error::InvalidMessage,
                "invalid polar ranges, nonfinite values, held flags, reserved bytes, zero tick and legacy shape fail before events");
            while(auto e = q.host.TakeEvent()) Check(e->kind != EventKind::GuestInput, "invalid input never reaches native backend");
        }
        for(unsigned defect = 0; defect < 5; ++defect)
        {
            auto invalid = Input(2);
            if(defect == 0) invalid.magnitude = std::numeric_limits<float>::infinity();
            if(defect == 1) invalid.magnitude = 1.01f;
            if(defect == 2) invalid.relativeRadians = std::numeric_limits<float>::quiet_NaN();
            if(defect == 3) invalid.referenceHeading = -4;
            if(defect == 4) invalid.clientTick = 0;
            Check(!p.guest.SubmitInput(invalid) && p.guest.CurrentPhase() == Phase::Running && !p.guest.TakeOutbound(),
                "invalid local producer input is refused without emitting or poisoning a valid connection");
        }
        Pair old; old.Begin(); auto legacy = Next(old.guest); legacy.payload[0] = 1;
        Check(old.host.Receive(11,legacy) == ReceiveResult::Rejected && old.host.LastError() == Error::InvalidMessage,
            "version1 peers are explicitly incompatible with the version2 polar temporal contract");
    }

    void OrderedTransitionsAndElision()
    {
        Pair p; p.Run(); auto baseline = Input(100); baseline.jumpHeld = false;
        Check(p.guest.SubmitInput(baseline), "first native observation establishes a held-state baseline");
        auto unchanged = baseline;
        for(std::uint32_t tick = 101; tick <= 104; ++tick)
        { unchanged.clientTick = tick; Check(p.guest.SubmitInput(unchanged), "identical native observation can be elided"); }
        auto press = baseline; press.clientTick = 105; press.jumpHeld = true;
        Check(p.guest.SubmitInput(press), "press transition queued before the next network send");
        unchanged = press;
        for(std::uint32_t tick = 106; tick <= 107; ++tick)
        { unchanged.clientTick = tick; Check(p.guest.SubmitInput(unchanged), "held sample does not overwrite original press tick"); }
        auto release = baseline; release.clientTick = 108; Check(p.guest.SubmitInput(release), "short held request release remains a separate event");
        auto stop = release; stop.clientTick = 109; stop.magnitude = 0; Check(p.guest.SubmitInput(stop), "movement stop remains ordered after request release");
        auto heading = stop; heading.clientTick = 110; heading.referenceHeading = 0.5f;
        Check(p.guest.SubmitInput(heading), "reference-heading change is retained even at zero magnitude");
        Check(p.guest.OutboundCount() == 5 && p.guest.LastGuestInputSequence() == 5, "only identical states were elided; no edge ordinal gaps");
        while(auto message = p.guest.TakeOutbound()) Accepted(p.host,*message);
        Check(p.host.EventCount() == 5 && !p.host.TakeOutbound(), "host queues every transition and synthesizes no movement or jump");
        const std::uint32_t ticks[] = {100,105,108,109,110};
        const bool held[] = {false,true,false,false,false};
        for(unsigned n = 0; n < 5; ++n)
        {
            auto e = NextEvent(p.host,EventKind::GuestInput);
            Check(e.sequence == n + 1 && e.input.clientTick == ticks[n] && e.input.jumpHeld == held[n],
                "press/release, original observation intervals and FIFO order survive one batched network update");
        }
        Check(!p.host.TakeEvent(), "host never folds request events back into the latest state");
        Pair zero; zero.Run(); InputIntent z; z.clientTick = 1;
        Check(zero.guest.SubmitInput(z), "zero baseline"); z.clientTick = 2; z.magnitude = -0.0f; z.relativeRadians = -0.0f; z.referenceHeading = -0.0f;
        Check(zero.guest.SubmitInput(z) && zero.guest.OutboundCount() == 1 && zero.guest.LastGuestInputSequence() == 1,
            "signed-zero representation changes do not create a false native transition");
        auto first = Next(zero.guest); Check(first.payload[24] == 1, "elision retains first observation tick instead of a latest-only mailbox tick");
    }

    void TransitionLossAndTransportBackpressure()
    {
        Pair upstream; upstream.Run(); Check(upstream.guest.SubmitInput(Input(1)), "first native observation before upstream loss");
        Check(!upstream.guest.SubmitInput(Input(3)) && upstream.guest.LastError() == Error::InputDiscontinuity &&
            upstream.guest.CurrentPhase() == Phase::Failed && upstream.guest.OutboundCount() == 0,
            "lost native observation fails before identical endpoint values can erase an unseen short press");
        for(unsigned gap = 0; gap < 2; ++gap)
        {
            Pair p; p.Run();
            for(std::uint32_t tick = 1; tick <= 3; ++tick) Check(p.guest.SubmitInput(Input(tick)), "prepare distinct ordered events");
            auto first = Next(p.guest), second = Next(p.guest), third = Next(p.guest);
            if(gap == 0)
                Check(p.host.Receive(11,second) == ReceiveResult::Rejected && p.host.LastError() == Error::InputDiscontinuity,
                    "dropping the baseline cannot turn the second event into a new first request");
            else
            {
                Accepted(p.host,first); Drain(p.host);
                Check(p.host.Receive(11,third) == ReceiveResult::Rejected && p.host.LastError() == Error::InputDiscontinuity,
                    "latest-only coalescing which loses a press is detected before a later release can apply");
            }
        }
        Pair redundant; redundant.Run(); Check(redundant.guest.SubmitInput(Input(1)), "prepare canonical transition baseline");
        auto event = Next(redundant.guest); Accepted(redundant.host,event); Drain(redundant.host);
        Set32(event.payload,16,2); Set32(event.payload,24,2);
        Check(redundant.host.Receive(11,event) == ReceiveResult::Rejected && redundant.host.LastError() == Error::InvalidMessage,
            "a fresh sequence with identical held state violates the elided transition-stream contract");
        Pair p; p.Run(); Check(p.guest.PeekOutbound() == nullptr && !p.guest.CommitOutbound(), "empty transport queue has no commit");
        Check(p.guest.SubmitInput(Input(1)), "baseline waits for transport acceptance");
        const auto original = p.guest.PeekOutbound()->payload;
        auto same = Input(1); same.clientTick = 2; Check(p.guest.SubmitInput(same), "identical sample while transport is blocked");
        Check(p.guest.SubmitInput(Input(3)), "another identical sample still cannot replace the blocked baseline");
        auto press = Input(4); Check(p.guest.SubmitInput(press), "edge queued behind a blocked baseline");
        Check(p.guest.OutboundCount() == 2 && p.guest.PeekOutbound()->payload == original,
            "failed transport send and later observations leave the exact front message unchanged");
        Accepted(p.host,*p.guest.PeekOutbound()); Check(p.guest.CommitOutbound(), "commit only after transport accepts full application message");
        auto baseline = NextEvent(p.host,EventKind::GuestInput); Check(baseline.sequence == 1 && baseline.input.clientTick == 1, "blocked baseline retained its original identity");
        Accepted(p.host,*p.guest.PeekOutbound()); Check(p.guest.CommitOutbound() && p.guest.PeekOutbound() == nullptr, "next edge commits once in order");
        auto request = NextEvent(p.host,EventKind::GuestInput);
        Check(request.sequence == 2 && request.input.clientTick == 4 && request.input.jumpHeld,
            "backpressure preserves the first tick of the queued held-request edge");
    }
}
int main()
{
    try
    {
        GoldenAndWaiting(); CompatibilityFailures(); InputAndAuthority(); InvalidStates();
        GenerationsAndRemoval(); MalformedAndBarrier(); BoundedQueuesAndFreshness();
        PolarInputValidation(); OrderedTransitionsAndElision(); TransitionLossAndTransportBackpressure();
        std::cout << "WinxCoop gameplay protocol: PASS " << checks << " checks\n"; return 0;
    }
    catch(const std::exception& e) { std::cerr << "WinxCoop gameplay protocol: FAIL " << e.what() << '\n'; return 1; }
}
