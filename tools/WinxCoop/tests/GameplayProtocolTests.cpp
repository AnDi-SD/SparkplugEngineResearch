#include "../src/Gameplay/GameplayProtocol.h"
#include <algorithm>
#include <cctype>
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
    void Check(bool b, const char* why) { ++checks; if(!b) throw std::runtime_error(why); }
    Config Configuration(SessionRole role)
    {
        Config c; c.role = role; c.compatibility.executable.fill(0x11); c.compatibility.adapter.fill(0x22);
        c.qualifiedLevel.levelId = 28; c.qualifiedLevel.entryId = 2;
        c.qualifiedLevel.resources.fill(0x33); c.qualifiedLevel.scenario.fill(0x44);
        c.hostSession = role == SessionRole::Host ? 0x1020304050607080ull : 0; return c;
    }
    void Set32(std::vector<std::uint8_t>& p, std::size_t at, std::uint32_t v)
    { for(unsigned n = 0; n < 4; ++n) p.at(at + n) = static_cast<std::uint8_t>(v >> (8*n)); }
    std::vector<std::uint8_t> Bytes(const char* text)
    {
        std::string s; for(auto p = text; *p; ++p) if(!std::isspace(static_cast<unsigned char>(*p))) s += *p;
        Check(s.size()%2 == 0,"complete independent literal"); std::vector<std::uint8_t> result;
        for(std::size_t i = 0; i < s.size(); i += 2) result.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));
        return result;
    }
    SessionMessage Next(GameplayProtocol& p)
    { auto m = p.TakeOutbound(); Check(m.has_value(),"expected outbound message"); return std::move(*m); }
    Event EventOf(GameplayProtocol& p, EventKind kind)
    { auto e = p.TakeEvent(); Check(e.has_value() && e->kind == kind,"expected typed event"); return *e; }
    void Drain(GameplayProtocol& p) { while(p.TakeEvent()) {} }
    void Receive(GameplayProtocol& p, const SessionMessage& m, std::uint64_t conn = 11)
    { Check(p.Receive(conn,m) == ReceiveResult::Accepted,"message accepted"); }
    struct Pair
    {
        Config h = Configuration(SessionRole::Host), g = Configuration(SessionRole::Guest);
        GameplayProtocol host{h}, guest{g};
        void Begin() { Check(host.Begin(11) && guest.Begin(11),"begin fresh connection"); }
        void Welcome() { Begin(); Receive(host,Next(guest)); Check(host.HostAssignLevel(7,h.qualifiedLevel),"host ready assigns lease"); Receive(guest,Next(host)); }
        void Run() { Welcome(); Check(guest.NotifyLocalReady(g.qualifiedLevel),"guest actually ready"); Receive(host,Next(guest)); Receive(guest,Next(host)); Drain(host); Drain(guest); }
    };
    AcceptedState State(std::uint32_t tick)
    { AcceptedState s; s.entity=1; s.hostTick=tick; s.position={{1,2,3}}; s.velocity={{4,0,0}}; s.animation=Animation::Walk; return s; }
    ObservedState Observation(std::uint32_t tick)
    { ObservedState s; s.entity=2; s.observationTick=tick; s.position={{1,2,3}}; s.velocity={{4,0,0}}; s.animation=Animation::Walk; return s; }
    Event Observe(Pair& p, std::uint32_t tick)
    { Check(p.guest.SubmitObservedState(Observation(tick)),"guest observes own stock state"); Receive(p.host,Next(p.guest)); return EventOf(p.host,EventKind::ObservedState); }
    void BarrierAndCompatibility()
    {
        Pair p; p.Begin(); auto hello=Next(p.guest);
        auto golden=Bytes("03000100 0000000000000000 00000000 01000000 00000000 "
            "1111111111111111111111111111111111111111111111111111111111111111 "
            "2222222222222222222222222222222222222222222222222222222222222222 "
            "1c000000 02000000 3333333333333333333333333333333333333333333333333333333333333333 "
            "4444444444444444444444444444444444444444444444444444444444444444");
        Check(hello.classId==0x31504357 && hello.key==1 && hello.payload==golden && golden.size()==160,"V3 Hello golden");
        Receive(p.host,hello); Check(!p.host.TakeOutbound() && p.host.CurrentPhase()==Phase::WaitingForHostLevel,"guest waits for host actual level");
        Check(!p.guest.HostAssignLevel(7,p.g.qualifiedLevel) && !p.guest.SubmitObservedState(Observation(1)),"guest cannot assign/start");
        Check(p.host.HostAssignLevel(7,p.h.qualifiedLevel),"host assigns lease"); auto welcome=Next(p.host);
        golden[2]=2; auto session=Bytes("8070605040302010"); std::copy(session.begin(),session.end(),golden.begin()+4); Set32(golden,12,7); golden.insert(golden.end(),{1,0,0,0,2,0,0,0});
        Check(welcome.payload==golden && welcome.payload.size()==168,"Welcome golden with qualified descriptor"); Receive(p.guest,welcome);
        auto wrong=p.g.qualifiedLevel; ++wrong.entryId;
        Check(!p.guest.NotifyLocalReady(wrong) && !p.guest.SubmitObservedState(Observation(1)),"wrong entry/loading not ready");
        Check(p.guest.NotifyLocalReady(p.g.qualifiedLevel) && p.guest.NotifyLocalReady(p.g.qualifiedLevel),"ready idempotent"); auto ready=Next(p.guest); Check(!p.guest.TakeOutbound(),"one Ready");
        Receive(p.host,ready); auto resume=Next(p.host); Check(p.guest.CurrentPhase()==Phase::Loading,"guest waits for Resume"); Receive(p.guest,resume);
        Check(p.host.LocalEntity()==1 && p.host.RemoteEntity()==2 && p.guest.LocalEntity()==2 && p.guest.RemoteEntity()==1,"remote model ownership explicit");
        Check(p.guest.Receive(11,resume)==ReceiveResult::IgnoredStale && !p.host.HostAssignLevel(8,p.h.qualifiedLevel),"duplicate barrier/new epoch cannot restart same lease");
        for(unsigned f=0;f<6;++f) {
            auto h=Configuration(SessionRole::Host),g=Configuration(SessionRole::Guest);
            if(f==0)++g.compatibility.executable[0]; if(f==1)++g.compatibility.adapter[0]; if(f==2)++g.qualifiedLevel.levelId;
            if(f==3)++g.qualifiedLevel.entryId; if(f==4)++g.qualifiedLevel.resources[0]; if(f==5)++g.qualifiedLevel.scenario[0];
            GameplayProtocol host(h),guest(g); Check(host.Begin(11)&&guest.Begin(11),"begin mismatching peers");
            Check(host.Receive(11,Next(guest))==ReceiveResult::Rejected && host.LastError()==Error::Incompatible && !host.TakeOutbound(),"all six fingerprints checked");
        }
    }
    void GoldenObservationAndAuthority()
    {
        Pair p; p.Run(); Check(p.guest.SubmitObservedState(Observation(42)),"submit observation"); auto raw=Next(p.guest);
        auto golden=Bytes("03000800 8070605040302010 07000000 01000000 02000000 2a000000 "
            "0000803f 00000040 00004040 00008040 00000000 00000000 "
            "0000803f 00000000 00000000 00000000 0000803f 00000000 00000000 00000000 0000803f 0100 0000");
        Check(raw.key==8 && raw.payload==golden && raw.payload.size()==92,"distinct ObservedState golden"); Receive(p.host,raw); auto observed=EventOf(p.host,EventKind::ObservedState);
        Check(observed.observationToken.connectionId==11 && observed.observationToken.sessionId==p.h.hostSession && observed.observationToken.levelEpoch==7 && observed.observationToken.sequence==1 && observed.observationToken.observationTick==42,"typed token owns full generation");
        Check(!p.host.TakeOutbound() && !p.host.TakeEvent() && p.host.PendingObservationCount()==1,"receiving does not accept or display");
        auto spoof=State(1); spoof.entity=2; Check(!p.host.PublishAcceptedState(spoof) && !p.guest.PublishAcceptedState(State(1)) && !p.guest.AcceptObservedState(observed.observationToken,1),"guest pose only through host decision");
        Check(p.host.AcceptObservedState(observed.observationToken,90),"host explicitly accepts"); auto accepted=Next(p.host);
        golden[2]=6; golden.erase(golden.begin()+24,golden.begin()+28); auto meta=Bytes("5a000000 01000000 2a000000"); golden.insert(golden.begin()+24,meta.begin(),meta.end());
        Check(accepted.key==6 && accepted.payload==golden && accepted.payload.size()==100,"accepted state golden with decision acknowledgement");
        auto display=EventOf(p.host,EventKind::AcceptedState); Check(display.entity==p.host.RemoteEntity() && display.state.position==observed.observed.position && display.state.acknowledgedObservation==1,"host remote guest model uses accepted pose");
        Receive(p.guest,accepted); auto receipt=EventOf(p.guest,EventKind::ObservationAccepted);
        Check(receipt.entity==p.guest.LocalEntity() && receipt.state.observedTick==42 && !p.guest.TakeEvent() && !p.guest.PendingObservationCount(),"own guest decision is ACK-only, no local-root application event");
        Check(!p.host.AcceptObservedState(observed.observationToken,91),"decision cannot repeat");
        Check(p.host.PublishAcceptedState(State(1)),"publish host own stock state"); auto host=Next(p.host); Receive(p.guest,host);
        Check(EventOf(p.guest,EventKind::AcceptedState).entity==p.guest.RemoteEntity(),"guest display event belongs to host only");
        Check(p.host.Receive(11,host)==ReceiveResult::Rejected && p.host.LastError()==Error::WrongRole,"guest cannot smuggle accepted wire state");
    }
    void FifoAndRejection()
    {
        Pair p; p.Run(); auto a=Observe(p,10),b=Observe(p,50),c=Observe(p,100);
        Check(!p.host.AcceptObservedState(b.observationToken,1),"decisions must be FIFO");
        for(unsigned f=0;f<6;++f) { auto t=a.observationToken; if(f==0)++t.connectionId; if(f==1)++t.sessionId; if(f==2)++t.levelEpoch; if(f==3)t.entity=1; if(f==4)++t.sequence; if(f==5)++t.observationTick;
            Check(!p.host.AcceptObservedState(t,1)&&!p.host.RejectObservedState(t,1),"every trusted token field checked"); }
        Check(p.host.PendingObservationCount()==3 && !p.host.TakeOutbound(),"bad trusted token does not consume decision");
        Check(p.host.RejectObservedState(a.observationToken,123) && p.host.AcceptObservedState(b.observationToken,1) && p.host.AcceptObservedState(c.observationToken,2),"mixed reject/accept decisions");
        auto rejected=Next(p.host),second=Next(p.host),third=Next(p.host);
        Check(rejected.key==9 && rejected.payload.size()==36 && std::vector<std::uint8_t>(rejected.payload.begin()+24,rejected.payload.end())==Bytes("01000000 0a000000 7b00 0000"),"explicit rejection golden");
        Receive(p.guest,rejected); Receive(p.guest,second); Receive(p.guest,third);
        Check(EventOf(p.guest,EventKind::ObservationRejected).rejectionReason==123,"rejection reason exposed");
        Check(EventOf(p.guest,EventKind::ObservationAccepted).observationToken.sequence==2 && EventOf(p.guest,EventKind::ObservationAccepted).observationToken.sequence==3 && !p.guest.TakeEvent(),"mixed decisions stay FIFO and never apply own root");
        Check(p.guest.LastDecidedObservationSequence()==3 && !p.guest.PendingObservationCount() && p.guest.Receive(11,rejected)==ReceiveResult::IgnoredStale,"decision receipts consumed once");
    }
    void InvalidPosesAndDecisions()
    {
        for(unsigned f=0;f<9;++f) { Pair p; p.Run(); Check(p.guest.SubmitObservedState(Observation(1)),"prepare malformed observation"); auto m=Next(p.guest);
            if(f==0)Set32(m.payload,28,0x7fc00000); if(f==1)Set32(m.payload,40,0x7f800000); if(f==2)Set32(m.payload,52,0x40000000); if(f==3)Set32(m.payload,52,0xbf800000);
            if(f==4)Set32(m.payload,56,0x3f800000); if(f==5)m.payload[88]=3; if(f==6)m.payload[90]=1; if(f==7)Set32(m.payload,24,0); if(f==8)Set32(m.payload,20,1);
            Check(p.host.Receive(11,m)==ReceiveResult::Rejected && p.host.CurrentPhase()==Phase::Failed && !p.host.TakeOutbound(),"nonfinite/reflection/skew/enum/reserved/tick/entity rejected before decision"); }
        for(unsigned f=0;f<8;++f) { Pair p; p.Run(); Check(p.host.PublishAcceptedState(State(1)),"prepare malformed accepted state"); auto m=Next(p.host);
            if(f==0)Set32(m.payload,36,0x7fc00000); if(f==1)Set32(m.payload,48,0x7f800000); if(f==2)Set32(m.payload,60,0x40000000); if(f==3)Set32(m.payload,60,0xbf800000);
            if(f==4)m.payload[96]=3; if(f==5)m.payload[98]=1; if(f==6)Set32(m.payload,28,1); if(f==7)Set32(m.payload,32,1);
            Check(p.guest.Receive(11,m)==ReceiveResult::Rejected && p.guest.LastError()==Error::InvalidMessage,"host wire state still validates pose and own ack fields"); }
        for(unsigned f=0;f<3;++f) { Pair p; p.Run(); auto e=Observe(p,42); Check(p.host.AcceptObservedState(e.observationToken,1),"prepare forged decision"); auto m=Next(p.host);
            if(f==0)Set32(m.payload,28,2); if(f==1)Set32(m.payload,32,43); if(f==2)Set32(m.payload,24,0);
            Check(p.guest.Receive(11,m)==ReceiveResult::Rejected,"unseen/different observation or invalid publication counter rejected"); }
        Pair policy; policy.Run(); auto finite=Observation(1); finite.position[0]=1e12f;
        Check(policy.guest.SubmitObservedState(finite),"finite envelope is explicitly not anti-cheat/collision validation"); Receive(policy.host,Next(policy.guest)); auto e=EventOf(policy.host,EventKind::ObservedState);
        Check(policy.host.RejectObservedState(e.observationToken,7),"host policy may reject structurally finite position");
    }
    void SequenceAndBackpressure()
    {
        for(unsigned f=0;f<3;++f) { Pair p; p.Run(); Check(p.guest.SubmitObservedState(Observation(10))&&p.guest.SubmitObservedState(Observation(20)),"two sampled observations"); auto a=Next(p.guest),b=Next(p.guest);
            if(f==0)Check(p.host.Receive(11,b)==ReceiveResult::Rejected,"missing baseline rejected"); else { Receive(p.host,a); Drain(p.host); if(f==1)Set32(b.payload,16,3); else Set32(b.payload,24,10); Check(p.host.Receive(11,b)==ReceiveResult::Rejected,"gap/backwards source tick rejected"); }
            Check(p.host.LastError()==Error::ObservationDiscontinuity,"typed observation discontinuity"); }
        Pair loss; loss.Run(); auto a=Observe(loss,1),b=Observe(loss,5); Check(loss.host.RejectObservedState(a.observationToken,1)&&loss.host.AcceptObservedState(b.observationToken,1),"mixed decision loss setup"); (void)Next(loss.host);
        Check(loss.guest.Receive(11,Next(loss.host))==ReceiveResult::Rejected && loss.guest.LastError()==Error::ObservationDiscontinuity,"lost rejection cannot skip to later acceptance");
        Pair p; p.Run(); Check(!p.guest.PeekOutbound()&&!p.guest.CommitOutbound(),"empty outbox"); Check(p.guest.SubmitObservedState(Observation(1)),"queue blocked observation"); auto front=p.guest.PeekOutbound()->payload;
        Check(p.guest.SubmitObservedState(Observation(5))&&p.guest.PeekOutbound()->payload==front,"later equal-pose sample cannot overwrite blocked front"); Receive(p.host,*p.guest.PeekOutbound()); Check(p.guest.CommitOutbound(),"commit after whole application-message acceptance"); Receive(p.host,*p.guest.PeekOutbound()); Check(p.guest.CommitOutbound()&&!p.guest.PeekOutbound(),"commit next in order");
        a=EventOf(p.host,EventKind::ObservedState); b=EventOf(p.host,EventKind::ObservedState); Check(a.observed.observationTick==1&&b.observed.observationTick==5&&b.observationToken.sequence==2,"sample gaps allowed without queued observation loss");
        Check(p.host.AcceptObservedState(a.observationToken,1),"block accepted decision"); front=p.host.PeekOutbound()->payload; Check(p.host.RejectObservedState(b.observationToken,1)&&p.host.PeekOutbound()->payload==front,"later rejection cannot overwrite accepted decision");
        Pair out; out.Run(); for(std::uint32_t i=1;i<=GameplayProtocol::MaximumOutbound;++i)Check(out.guest.SubmitObservedState(Observation(i)),"fill outbox");
        Check(!out.guest.SubmitObservedState(Observation(100))&&out.guest.LastError()==Error::Backpressure&&!out.guest.PendingObservationCount()&&!out.guest.OutboundCount(),"outbox overflow terminal and invalidates pending");
        Pair pending; pending.Run(); SessionMessage last; for(std::uint32_t i=1;i<=GameplayProtocol::MaximumPendingObservations;++i) { Check(pending.guest.SubmitObservedState(Observation(i)),"fill undecided backlog"); last=Next(pending.guest); Receive(pending.host,last); Drain(pending.host); }
        Set32(last.payload,16,17); Set32(last.payload,24,17); Check(pending.host.Receive(11,last)==ReceiveResult::Rejected&&pending.host.LastError()==Error::Backpressure&&!pending.host.PendingObservationCount(),"host decision queue bounded after event delivery");
        Check(!pending.guest.SubmitObservedState(Observation(17))&&pending.guest.LastError()==Error::Backpressure,"guest unacknowledged backlog bounded after transport commit");
        Pair events; events.Run(); for(std::uint32_t i=1;i<=GameplayProtocol::MaximumEvents;++i) { Check(events.host.PublishAcceptedState(State(i)),"fill render events"); Receive(events.guest,Next(events.host)); }
        Check(events.host.PublishAcceptedState(State(100)),"one additional pose"); Check(events.guest.Receive(11,Next(events.host))==ReceiveResult::Rejected&&events.guest.LastError()==Error::Backpressure,"render event queue bounded");
    }
    void GenerationsRemovalAndSourceIds()
    {
        Pair p; p.Run(); Check(p.guest.SubmitObservedState(Observation(1)),"generation-bound pose"); auto m=Next(p.guest);
        Check(p.host.Receive(99,m)==ReceiveResult::WrongConnection&&!p.host.PendingObservationCount(),"foreign carrier ignored"); auto stale=m; ++stale.payload[4]; Check(p.host.Receive(11,stale)==ReceiveResult::IgnoredStale,"old session ignored"); stale=m; Set32(stale.payload,12,6); Check(p.host.Receive(11,stale)==ReceiveResult::IgnoredStale,"old epoch ignored"); Receive(p.host,m); auto e=EventOf(p.host,EventKind::ObservedState);
        Check(p.host.RemoveEntity(2,4)&&!p.host.PendingObservationCount()&&!p.host.AcceptObservedState(e.observationToken,1),"removal invalidates undecided pose"); Receive(p.guest,Next(p.host)); Check(!p.guest.PendingObservationCount()&&!p.guest.SubmitObservedState(Observation(2)),"removed guest no longer submits"); Check(EventOf(p.guest,EventKind::EntityRemoved).removalReason==4,"removal event");
        Pair close; close.Run(); e=Observe(close,1); close.host.Close(); Check(!close.host.PendingObservationCount()&&!close.host.AcceptObservedState(e.observationToken,1)&&!close.host.Begin(12),"terminal runtime cannot reuse token"); (void)EventOf(close.host,EventKind::EntityRemoved); (void)EventOf(close.host,EventKind::Disconnected); close.host.Close(); Check(!close.host.TakeEvent(),"one terminal event set");
        auto h=Configuration(SessionRole::Host),g=Configuration(SessionRole::Guest); GameplayProtocol host(h),guest(g); Check(host.Begin(111)&&guest.Begin(222),"endpoint-local carrier IDs"); Receive(host,Next(guest),111); Check(host.HostAssignLevel(7,h.qualifiedLevel),"assign distinct-ID lease"); Receive(guest,Next(host),222); Check(guest.NotifyLocalReady(g.qualifiedLevel),"guest ready"); Receive(host,Next(guest),111); Receive(guest,Next(host),222); Drain(host); Drain(guest);
        Check(guest.SubmitObservedState(Observation(1)),"distinct-ID pose"); Receive(host,Next(guest),111); auto token=EventOf(host,EventKind::ObservedState).observationToken; Check(token.connectionId==111&&host.AcceptObservedState(token,1),"host decision token uses host-local carrier"); Receive(guest,Next(host),222); Check(EventOf(guest,EventKind::ObservationAccepted).observationToken.connectionId==222,"guest receipt uses guest-local carrier");
    }
    void MalformedStaleAndRetiredInput()
    {
        for(unsigned f=0;f<8;++f) { Pair p; p.Begin(); auto m=Next(p.guest); if(f==0)++m.classId; if(f==1)++m.key; if(f==2)m.payload[3]=1; if(f==3)Set32(m.payload,16,0); if(f==4)m.payload.pop_back(); if(f==5)m.payload.push_back(0); if(f==6)Set32(m.payload,20,3); if(f==7)m.payload[2]=10; Check(p.host.Receive(11,m)==ReceiveResult::Rejected&&p.host.CurrentPhase()==Phase::Failed,"malformed packet rejected"); }
        for(auto version:{std::uint8_t(1),std::uint8_t(2)}) { Pair p; p.Begin(); auto m=Next(p.guest); m.payload[0]=version; Check(p.host.Receive(11,m)==ReceiveResult::Rejected,"old wire version rejected"); }
        Pair p; p.Run(); InputIntent old; old.clientTick=1; Check(!p.guest.SubmitInput(old)&&!p.host.SubmitInput(old)&&!p.guest.LastGuestInputSequence()&&!p.guest.TakeOutbound(),"V3 legacy input API explicitly inactive");
        SessionMessage retired; retired.classId=GameplayProtocol::ClassId; retired.key=5; retired.payload=Bytes("03000500 8070605040302010 07000000 01000000 02000000"); retired.payload.resize(44);
        Check(p.host.Receive(11,retired)==ReceiveResult::Rejected&&p.host.LastError()==Error::InvalidMessage,"V3 input wire kind retired");
        Pair future; future.Run(); Check(future.guest.SubmitObservedState(Observation(1)),"future epoch setup"); auto m=Next(future.guest); Set32(m.payload,12,8); Check(future.host.Receive(11,m)==ReceiveResult::Rejected&&future.host.LastError()==Error::InvalidEpoch,"guest cannot advance host epoch");
        Pair replay; replay.Run(); Check(replay.guest.SubmitObservedState(Observation(1)),"duplicate observation setup"); m=Next(replay.guest); Receive(replay.host,m); Drain(replay.host); Check(replay.host.Receive(11,m)==ReceiveResult::IgnoredStale&&!replay.host.TakeEvent()&&replay.host.PendingObservationCount()==1,"observation replay no second decision event");
        Pair hostpose; hostpose.Run(); Check(hostpose.host.PublishAcceptedState(State(10)),"host state freshness baseline"); m=Next(hostpose.host); Receive(hostpose.guest,m); Drain(hostpose.guest); Set32(m.payload,16,2); Set32(m.payload,24,9); Check(hostpose.guest.Receive(11,m)==ReceiveResult::IgnoredStale&&!hostpose.guest.TakeEvent(),"stale host pose not displayed");
        Check(hostpose.guest.SubmitObservedState(Observation(std::numeric_limits<std::uint32_t>::max()))&&!hostpose.guest.SubmitObservedState(Observation(0))&&!hostpose.guest.SubmitObservedState(Observation(1)),"native observation tick nonwrapping");
        bool invalid=false; try { auto c=Configuration(SessionRole::Guest); c.hostSession=1; GameplayProtocol bad(c); } catch(const std::invalid_argument&) { invalid=true; } Check(invalid,"guest cannot preassign host session");
    }
}
int main()
{
    try { BarrierAndCompatibility(); GoldenObservationAndAuthority(); FifoAndRejection(); InvalidPosesAndDecisions(); SequenceAndBackpressure(); GenerationsRemovalAndSourceIds(); MalformedStaleAndRetiredInput(); std::cout<<"WinxCoop gameplay protocol V3: PASS "<<checks<<" checks\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<"WinxCoop gameplay protocol V3: FAIL "<<e.what()<<'\n'; return 1; }
}
