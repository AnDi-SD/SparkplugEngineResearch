#include "../src/Gameplay/GameplayProtocol.h"
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace winxcoop::gameplay;
using winxcoop::networking::SessionMessage;
using winxcoop::networking::SessionRole;
namespace
{
    unsigned checks{};
    void Check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
    Config Configuration(SessionRole role,bool follow=true)
    {
        Config c;c.role=role;c.compatibility.executable.fill(0x11);c.compatibility.adapter.fill(0x22);
        c.qualifiedLevel.resources.fill(0x33);c.qualifiedLevel.scenario.fill(0x44);
        c.allowDeferredLevel=true;c.allowSceneFollow=follow;
        c.hostSession=role==SessionRole::Host?0x1020304050607080ull:0;return c;
    }
    LevelDescriptor Level(std::uint32_t id)
    {auto l=Configuration(SessionRole::Host).qualifiedLevel;l.levelId=id;return l;}
    void Set32(std::vector<std::uint8_t>& p,std::size_t at,std::uint32_t value)
    {for(unsigned i=0;i<4;++i)p.at(at+i)=static_cast<std::uint8_t>(value>>(8*i));}
    std::uint32_t Word(const SessionMessage& m,std::size_t at)
    {return std::uint32_t(m.payload.at(at))|(std::uint32_t(m.payload.at(at+1))<<8)|(std::uint32_t(m.payload.at(at+2))<<16)|(std::uint32_t(m.payload.at(at+3))<<24);}
    SessionMessage Next(GameplayProtocol& p)
    {auto m=p.TakeOutbound();Check(m.has_value(),"outbound exists");return std::move(*m);}
    void Receive(GameplayProtocol& p,const SessionMessage& m)
    {Check(p.Receive(11,m)==ReceiveResult::Accepted,"valid frame accepted");}
    void Drain(GameplayProtocol& p){while(p.TakeEvent()){} }
    Event EventOf(GameplayProtocol& p,EventKind kind)
    {auto e=p.TakeEvent();Check(e.has_value()&&e->kind==kind,"typed event order");return *e;}
    ObservedState Pose(std::uint32_t tick)
    {ObservedState p;p.entity=2;p.observationTick=tick;p.position={{1,2,3}};return p;}
    AcceptedState State(std::uint32_t tick)
    {AcceptedState p;p.entity=1;p.hostTick=tick;p.position={{4,5,6}};return p;}
    struct Pair
    {
        Config h,g;GameplayProtocol host,guest;SessionMessage initialWelcome;
        explicit Pair(bool follow=true):h(Configuration(SessionRole::Host,follow)),g(Configuration(SessionRole::Guest,follow)),host(h),guest(g){}
        void Run(std::uint32_t epoch=1)
        {
            Check(host.Begin(11)&&guest.Begin(11),"fresh carrier");Receive(host,Next(guest));
            Check(host.HostAssignLevel(epoch,Level(1)),"actual initial host scene");initialWelcome=Next(host);Receive(guest,initialWelcome);
            Resume();
        }
        void Resume()
        {
            Check(guest.NotifyLocalReady(guest.AssignedLevel()),"actual guest readiness");Receive(host,Next(guest));Receive(guest,Next(host));
            Check(host.CurrentPhase()==Phase::Running&&guest.CurrentPhase()==Phase::Running,"both running");Drain(host);Drain(guest);
        }
    };
    void FenceAndNewEpoch()
    {
        Pair p;p.Run();
        Check(p.guest.SubmitObservedState(Pose(10)),"old observation1");Receive(p.host,Next(p.guest));
        const auto oldToken=EventOf(p.host,EventKind::ObservedState).observationToken;
        Check(p.guest.SubmitObservedState(Pose(20)),"old observation2 in flight");auto lateObservation=Next(p.guest);
        Check(p.host.AcceptObservedState(oldToken,100),"old decision queued");
        const auto oldState=*p.host.PeekOutbound();
        Check(p.host.PublishAcceptedState(State(100)),"old host display queued");
        Check(p.host.HostSuspendLevel(0x1234),"native retirement fence");
        Check(p.host.HostSuspendLevel(0x1234)&&!p.host.HostSuspendLevel(5),"same immutable suspension idempotent");
        const auto he=EventOf(p.host,EventKind::SceneSuspended);
        Check(he.epoch==1&&he.suspensionReason==0x1234&&he.cancelledMessages==2,"old unsent data explicitly cancelled");
        Check(!p.host.AcceptObservedState(oldToken,101)&&!p.host.PublishAcceptedState(State(101)),"old scene cannot publish or decide");
        auto fence=Next(p.host);
        const std::vector<std::uint8_t> golden{4,0,10,0,0x80,0x70,0x60,0x50,0x40,0x30,0x20,0x10,1,0,0,0,1,0,0,0,0,0,0,0,0x34,0x12,0,0};
        Check(fence.key==10&&fence.payload==golden,"independent V4 SceneSuspend golden");
        Receive(p.guest,fence);const auto ge=EventOf(p.guest,EventKind::SceneSuspended);
        Check(ge.cancelledObservations==2&&p.guest.PendingObservationCount()==0&&!p.guest.EntityAlive(1)&&!p.guest.EntityAlive(2),"guest FIFO and entities invalidated explicitly");
        Check(p.host.Receive(11,lateObservation)==ReceiveResult::IgnoredStale&&p.host.LastError()==Error::None,"in-flight old observation ignored before FIFO validation");
        Check(p.guest.Receive(11,fence)==ReceiveResult::IgnoredStale&&!p.guest.TakeEvent(),"same fence replay emits no duplicate event");
        Check(p.guest.Receive(11,p.initialWelcome)==ReceiveResult::IgnoredStale,"old Welcome cannot recreate suspended scene");
        Check(!p.guest.SubmitObservedState(Pose(1))&&!p.guest.NotifyLocalReady(Level(1)),"guest cannot be ready or move in suspended scene");
        Check(!p.host.HostAssignLevel(1,Level(3))&&p.host.SceneSuspended(),"same epoch cannot resume");
        auto bad=Level(3);bad.resources[0]^=1;
        Check(!p.host.HostAssignLevel(2,bad)&&p.host.SceneSuspended(),"new scene retains capability admission");
        Check(p.host.HostAssignLevel(2,Level(3)),"new actual host scene assigned");auto welcome=Next(p.host);
        Check(Word(welcome,12)==2&&Word(welcome,16)==1,"new epoch has fresh data sequence");Receive(p.guest,welcome);
        Check(p.guest.SessionId()==p.h.hostSession&&p.guest.ConnectionId()==11&&p.guest.LevelEpoch()==2&&p.guest.AssignedLevel()==Level(3),"same carrier/session owns new scene");
        Check(!p.guest.SubmitObservedState(Pose(1)),"fresh epoch waits new native Ready");
        p.Resume();auto poison=oldState;Set32(poison.payload,16,0xffffffffu);
        Check(p.guest.Receive(11,poison)==ReceiveResult::IgnoredStale,"old high sequence cannot poison new epoch");
        Check(p.guest.SubmitObservedState(Pose(1)),"new epoch source tick starts at1");auto current=Next(p.guest);
        Check(Word(current,16)==1&&Word(current,12)==2,"new observation sequence1 bound to new epoch");Receive(p.host,current);
        const auto token=EventOf(p.host,EventKind::ObservedState).observationToken;
        Check(token.levelEpoch==2&&token.sequence==1&&!p.host.AcceptObservedState(oldToken,1)&&p.host.AcceptObservedState(token,1),"new FIFO baseline does not admit old token");
        Receive(p.guest,Next(p.host));Check(EventOf(p.guest,EventKind::ObservationAccepted).observationToken.levelEpoch==2,"new receipt belongs only to new epoch");
    }
    void QueuedControlsAndLoading()
    {
        Pair p;p.Run();
        Check(p.host.HostSuspendLevel(1)&&p.host.HostAssignLevel(2,Level(1)),"same-level new scene queued after fence");
        Check(p.host.HostSuspendLevel(2)&&p.host.HostAssignLevel(4,Level(2)),"loading can suspend; actual native generation may skip an unobserved scene");
        const std::uint32_t kinds[]{10,2,10,2},epochs[]{1,2,2,4};
        for(unsigned i=0;i<4;++i){auto m=Next(p.host);Check(m.key==kinds[i]&&Word(m,12)==epochs[i],"queued control order retained across rapid scenes");Receive(p.guest,m);}
        Check(!p.host.TakeOutbound()&&p.guest.LevelEpoch()==4&&!p.guest.LocalReady(),"latest assigned epoch still waits native barrier");
        Check(EventOf(p.guest,EventKind::SceneSuspended).epoch==1,"old fence remains observable");
        Check(EventOf(p.guest,EventKind::LevelAssigned).epoch==2,"intermediate assignment retained in control order");
        Check(EventOf(p.guest,EventKind::SceneSuspended).epoch==2,"loading fence retained");
        Check(EventOf(p.guest,EventKind::LevelAssigned).epoch==4,"latest assignment retained");p.Resume();
    }
    void BeforeFirstWelcomeAndDisabledFollow()
    {
        Pair p;Check(p.host.Begin(11)&&p.guest.Begin(11),"deferred handshake");
        Check(p.host.HostAssignLevel(1,Level(1))&&p.host.HostSuspendLevel(8)&&!p.host.TakeOutbound(),"unannounced native scene needs no peer fence");
        Check(p.host.HostAssignLevel(2,Level(3)),"new ready before first Hello");Receive(p.host,Next(p.guest));auto welcome=Next(p.host);
        Check(welcome.key==2&&Word(welcome,12)==2,"first Welcome names actual latest scene");Receive(p.guest,welcome);p.Resume();
        Pair fixed(false);fixed.Run();Check(!fixed.host.HostSuspendLevel()&&!fixed.host.HostAssignLevel(2,Level(1))&&fixed.host.CurrentPhase()==Phase::Running,"fixed scene behavior remains explicit");
        bool bad=false;try{auto c=Configuration(SessionRole::Host);c.allowDeferredLevel=false;c.qualifiedLevel=Level(1);GameplayProtocol invalid(c);}catch(const std::invalid_argument&){bad=true;}
        Check(bad,"follow needs deferred scene capability");
    }
    void PendingCancellationAndLimits()
    {
        Pair p;p.Run();for(unsigned i=1;i<=3;++i)Check(p.guest.SubmitObservedState(Pose(i)),"queued uncommitted observation");
        Check(p.host.HostSuspendLevel(1),"fence with queued client data");Receive(p.guest,Next(p.host));const auto e=EventOf(p.guest,EventKind::SceneSuspended);
        Check(e.cancelledObservations==3&&e.cancelledMessages==3&&!p.guest.TakeOutbound(),"queued observations cancelled, not acknowledged or carried forward");
        Pair maximum;maximum.Run(std::numeric_limits<std::uint32_t>::max());Check(maximum.host.HostSuspendLevel(),"maximum epoch can suspend safely");
        Check(!maximum.host.HostAssignLevel(0,Level(2))&&!maximum.host.HostAssignLevel(1,Level(2))&&maximum.host.SceneSuspended(),"no epoch wrap/reuse");
        Pair cap;cap.Run();for(unsigned n=2;n<=5;++n)Check(cap.host.HostSuspendLevel()&&cap.host.HostAssignLevel(n,Level(n)),"fill durable control queue");
        Check(!cap.host.HostSuspendLevel()&&cap.host.LastError()==Error::Backpressure&&cap.host.CurrentPhase()==Phase::Failed,"control queue cannot silently discard required fences");
    }
    void MalformedTransitions()
    {
        for(unsigned field=0;field<5;++field)
        {
            Pair p;p.Run();Check(p.host.HostSuspendLevel(),"prepare bad fence");auto frame=Next(p.host);
            if(field==0)Set32(frame.payload,20,1);
            if(field==1)frame.payload[26]=1;
            if(field==2)Set32(frame.payload,12,2);
            if(field==3)frame.payload[0]=3;
            if(field==4)frame.payload.pop_back();
            Check(p.guest.Receive(11,frame)==ReceiveResult::Rejected&&p.guest.CurrentPhase()==Phase::Failed,"malformed/entity/future/old-version fence rejected");
        }
        Pair role;role.Run();Check(role.host.HostSuspendLevel(),"role test fence");Check(role.host.Receive(11,Next(role.host))==ReceiveResult::Rejected&&role.host.LastError()==Error::WrongRole,"guest cannot suspend host");
        Pair future;future.Run();auto welcome=future.initialWelcome;Set32(welcome.payload,12,2);
        Check(future.guest.Receive(11,welcome)==ReceiveResult::Rejected&&future.guest.LastError()==Error::InvalidEpoch,"future Welcome requires an explicit prior fence");
        Pair duplicate;duplicate.Run();Check(duplicate.host.HostSuspendLevel(),"duplicate setup");auto frame=Next(duplicate.host);Receive(duplicate.guest,frame);Set32(frame.payload,16,2);
        Check(duplicate.guest.Receive(11,frame)==ReceiveResult::Rejected,"different sequence cannot repeat an already suspended epoch");
        Pair disabled(false),source;disabled.Run();source.Run();Check(source.host.HostSuspendLevel(),"disabled receiver setup");
        Check(disabled.guest.Receive(11,Next(source.host))==ReceiveResult::Rejected,"receiver without follow never adopts scene suspension");
    }
}
int main()
{
    try{FenceAndNewEpoch();QueuedControlsAndLoading();BeforeFirstWelcomeAndDisabledFollow();PendingCancellationAndLimits();MalformedTransitions();std::cout<<"WinxCoop scene-follow protocol V4: PASS "<<checks<<" checks\n";return 0;}
    catch(const std::exception& error){std::cerr<<"WinxCoop scene-follow protocol V4: FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
