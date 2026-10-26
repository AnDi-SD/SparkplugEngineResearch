#include "Code/wxMenuGameFlowState.h"
#include "Code/SparkBase/spNotification.h"
#include "Code/Sparkplug/spResource.h"
#include "Analysis/PC/wxMenuGameFlowStateAbi.h"
#include "Analysis/PS2/wxMenuGameFlowStateAbi.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
    void* Token(unsigned value){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));}
    unsigned Number(void* value){return static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(value));}
    struct Host final : wxMenuGameFlowStateHost
    {
        struct Object final : spResource
        {
            Host& host;unsigned id;
            Object(Host& h,unsigned i):host(h),id(i){}
            ~Object() override
            {
                if(!host.active)return;
                host.events.push_back("D"+std::to_string(id));
                if(host.flags&1)
                {
                    auto& s=host.active->MenuStateForFixture();
                    s.object4C=host.objects[1];s.allocation3C=Token(42);s.timer50=host.timerB;
                }
            }
        };
        wxGameFlowResourcesForAnalysis resources;
        std::array<std::unique_ptr<spBaseObject>,2> objectOwners;
        std::array<spBaseObject*,2> objects{};
        std::vector<std::unique_ptr<spTaskTimer>> timers;
        std::set<spTaskTimer*> ownAddresses;
        spTaskTimer* timerA=nullptr;spTaskTimer* timerB=nullptr;
        wxMenuGameFlowState* active=nullptr;
        std::set<wxMenuGameFlowState*> listeners;
        std::array<bool,2> bufferAlive{true,true};
        std::array<std::uint8_t,2> deviceFlags{0,1},shared{90,90};
        std::array<unsigned,2> hookIds{11,13};
        std::vector<std::string> events;
        HookProfileForAnalysis profile=HookProfileForAnalysis::PC;
        unsigned owner=1,gui=1,sharedID=1,clock=0,reg=0,unreg=0;
        std::uint32_t flags=0;bool fixedClock=false;
        std::string unsubscribe="null";
        Host()
        {
            for(unsigned i=0;i<2;++i){objectOwners[i]=std::make_unique<Object>(*this,i+1);objects[i]=objectOwners[i].get();}
            auto timer=std::make_unique<spTaskTimer>();timerB=timer.get();
            timer->SetStateForAnalysis({true,true,333,777,222,1.5f});timers.push_back(std::move(timer));
        }
        ~Host() override{active=nullptr;}
        void* BorrowInputForAnalysis() override{return Token(owner);}
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override
        {
            ++clock;const auto sample=fixedClock||clock%2?1200u:1300u;
            events.push_back("C"+std::to_string(sample));
            if(clock==2&&(flags&32))active->MenuStateForFixture().timer50=timerB;
            return {sample,1};
        }
        wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() override{return resources;}
        HookProfileForAnalysis MenuHookProfileForAnalysis() const noexcept override{return profile;}
        void RegisterMenuReceiverForAnalysis(wxMenuGameFlowState& value,std::uint32_t tag) override
        {Check(tag==32&&listeners.insert(&value).second,"Register exact tag20");++reg;active=&value;events.push_back("+20");if(flags&64)owner=2;}
        void UnregisterMenuReceiverForAnalysis(wxMenuGameFlowState& value,std::uint32_t tag) override
        {
            Check(tag==32&&listeners.erase(&value)==1,"Unregister exact receiver/tag20");++unreg;active=&value;events.push_back("-20");
            auto& s=value.MenuStateForFixture();
            if(flags&16){s.object4C=objects[1];s.allocation3C=Token(42);s.timer50=Alive(timerA)?timerA:timerB;}
            std::ostringstream out;out<<"{\"object\":"<<ObjectID(s.object4C)<<",\"buffer\":"<<BufferID(s.allocation3C)<<",\"timer\":"<<TimerID(s.timer50)<<'}';unsubscribe=out.str();
        }
        bool Alive(spTaskTimer* pointer) const{for(const auto& t:timers)if(t.get()==pointer)return true;return false;}
        unsigned TimerID(spTaskTimer* p) const{if(!p)return 0;if(ownAddresses.count(p))return 1;if(p==timerB)return 2;throw std::runtime_error("Unknown timer");}
        unsigned ObjectID(spBaseObject* p) const{if(!p)return 0;for(unsigned i=0;i<2;++i)if(p==objects[i])return i+1;throw std::runtime_error("Unknown object");}
        unsigned BufferID(void* p) const{if(!p)return 0;Check(p==Token(41)||p==Token(42),"Known raw allocation");return Number(p)-40;}
        void StoreMenuTimerAllocationForAnalysis(std::unique_ptr<spTaskTimer> timer) override
        {timerA=timer.get();ownAddresses.insert(timerA);timers.push_back(std::move(timer));}
        std::unique_ptr<spTaskTimer> TakeMenuTimerAllocationForAnalysis(spTaskTimer& timer) override
        {
            for(auto& t:timers)if(t.get()==&timer){events.push_back("T"+std::to_string(TimerID(&timer)));return std::move(t);}
            throw std::runtime_error("Timer Take must refer to live arena storage");
        }
        std::unique_ptr<spBaseObject> TakeMenuObjectAllocationForAnalysis(spBaseObject& object) override
        {for(auto& p:objectOwners)if(p.get()==&object)return std::move(p);throw std::runtime_error("Object Take must refer to live arena storage");}
        void FreeMenuAllocationForAnalysis(void* p) override
        {
            const auto id=BufferID(p);Check(id&&bufferAlive[id-1],"Free exact live raw allocation once");bufferAlive[id-1]=false;events.push_back("B"+std::to_string(id));
            if(flags&2)active->MenuStateForFixture().timer50=timerB;
            if(flags&4)active->MenuStateForFixture().allocation3C=Token(42);
        }
        void* ResolveMenuGameContextForAnalysis() override{return Token(51);}
        void* ReadMenuNotificationTarget2B4ForAnalysis(void* p) override{Check(p==Token(51),"Context receiver");return flags&1048576?nullptr:Token(52);}
        void DispatchNotificationForAnalysis(void* target,const spNotificationForAnalysis& n) override
        {
            Check(target==Token(52)&&n.code==0x2764&&n.sender==active&&n.word04==0&&n.word08==0&&n.word0C==0&&n.word14==0&&n.payload==0&&n.tail==0,"Exact eight logical notification words");events.push_back("N");
            if(flags&512){active->MenuStateForFixture().view40=7;active->MutableStateForAnalysis().field25=0xEE;}
        }
        void* ResolveMenuGUIManagerForAnalysis() override{return Token(20+gui);}
        void SetMenuViewEnabledForAnalysis(void* manager,std::uint32_t view,std::uint32_t enabled) override
        {events.push_back("V"+std::to_string(Number(manager)-20)+':'+std::to_string(view)+':'+std::to_string(enabled));if(flags&1024)gui=2;}
        void* Lookup(void* manager,std::uint32_t view,std::string suffix)
        {
            const auto id=Number(manager)-20;events.push_back("L"+std::to_string(id)+':'+std::to_string(view)+suffix);
            if(flags&4096)owner=2;
            if(flags&8192){gui=2;active->MenuStateForFixture().view40=9;}
            return Token(30+id);
        }
        void* FindMenuWindowPCForAnalysis(void* manager,std::uint32_t view,std::uint32_t flag) override{return Lookup(manager,view,':'+std::to_string(flag));}
        void* FindMenuWindowPS2ForAnalysis(void* manager,std::uint32_t view) override{return Lookup(manager,view,"");}
        void Window(void* p,std::string suffix){events.push_back("W"+std::to_string(Number(p)-30)+suffix);if(flags&16384)owner=2;}
        void ApplyMenuWindowPCForAnalysis(void* p) override{Window(p,"");}
        void ApplyMenuWindowPS2ForAnalysis(void* p,std::uint32_t flag) override{Window(p,':'+std::to_string(flag));}
        void SetMenuHUDForAnalysis(void* manager,std::uint32_t value) override
        {events.push_back("H"+std::to_string(Number(manager)-20)+':'+std::to_string(value));if(flags&32768)active->MutableStateForAnalysis().field25=0xAA;}
        void* ResolveMenuAudioInterfaceForAnalysis() override{return Token(61);}
        void* GetMenuAudioControllerForAnalysis(void* p) override{Check(p==Token(61),"Audio subobject receiver");events.push_back("A");return Token(62);}
        void SetMenuAudioEnabledForAnalysis(void* p,std::uint32_t value) override
        {Check(p==Token(62),"Audio controller receiver");events.push_back("E"+std::to_string(value));if(flags&2048)active->MutableStateForAnalysis().field25=0xEE;}
        void* ResolveMenuExitServiceForAnalysis() override{return Token(63);}
        void InvokeMenuExitServiceForAnalysis(void* p,std::uint32_t value) override
        {Check(p==Token(63)&&value==1,"Exit service exact receiver/value");events.push_back("X1");if(flags&262144)owner=2;}
        void* ResolveMenuInputOwnerForAnalysis() override{return Token(70+owner);}
        void* ReadMenuInput1CForAnalysis(void* p) override
        {if(flags&524288)active->MenuStateForFixture().mode44=10;return Token(hookIds[Number(p)-71]);}
        void* ReadMenuInput18ForAnalysis(void* p) override{return Token(Number(p)-70);}
        std::uint8_t ReadMenuInputByte18ForAnalysis(void* p) override
        {const auto id=Number(p);const auto value=deviceFlags[id-1];events.push_back("I"+std::to_string(id)+':'+std::to_string(value));if(flags&131072)sharedID=2;return value;}
        void ResetMenuInputForAnalysis(void* p) override
        {
            events.push_back("R"+std::to_string(Number(p)-10));
            if(flags&128){hookIds[0]=12;owner=2;}
            if(flags&256){auto& s=active->MenuStateForFixture();s.mode44=0xDEADBEEF;s.view40=7;active->MutableStateForAnalysis().field25=0xA5;}
        }
        void SetMenuInputModeForAnalysis(void* p,std::uint32_t mode) override{events.push_back("M"+std::to_string(Number(p)-10)+':'+std::to_string(mode));}
        std::uint8_t& BorrowMenuSharedFlag45ForAnalysis() override{return shared[sharedID-1];}
        std::string Snapshot(const wxMenuGameFlowState& object) const
        {
            const auto& s=object.GetMenuStateForAnalysis();std::ostringstream out;
            out<<"{\"view\":"<<s.view40<<",\"mode\":"<<s.mode44<<",\"field25\":"<<unsigned(object.GetStateForAnalysis().field25)
                <<",\"cached\":"<<Number(s.borrowedInput54)<<",\"field58\":"<<unsigned(s.field58)<<",\"object\":"<<ObjectID(s.object4C)
                <<",\"buffer\":"<<BufferID(s.allocation3C)<<",\"timer\":"<<TimerID(s.timer50)<<",\"timerState\":";
            if(s.timer50)
            {
                const auto& t=s.timer50->GetStateForAnalysis();std::uint32_t bits;std::memcpy(&bits,&t.deltaSeconds,sizeof(bits));
                out<<'['<<t.active<<','<<t.relative<<','<<t.currentMilliseconds<<','<<t.startMilliseconds<<','<<t.pausedMilliseconds<<','<<bits<<']';
            }
            else out<<"null";
            out<<",\"shared\":["<<unsigned(shared[0])<<','<<unsigned(shared[1])<<"]}";return out.str();
        }
    };
    std::string Events(const Host& host)
    {std::ostringstream out;out<<'[';for(std::size_t i=0;i<host.events.size();++i){if(i)out<<',';out<<'"'<<host.events[i]<<'"';}return out.str()+']';}
    void Emit(bool ps2)
    {
        std::string line;
        while(std::getline(std::cin,line))
        {
            if(line.empty())continue;Host host;std::string op;std::uint32_t view,mode,a,b,objectID,bufferID,field25;
            std::istringstream in(line);Check(bool(in>>op>>view>>mode>>a>>b>>host.flags>>objectID>>bufferID>>field25),"Nine Menu input tokens");
            Check(objectID<=2&&bufferID<=2&&a<256&&b<256&&field25<256,"Menu input fixture ranges");host.deviceFlags={std::uint8_t(a),std::uint8_t(b)};
            host.profile=ps2?Host::HookProfileForAnalysis::PS2:Host::HookProfileForAnalysis::PC;
            auto object=std::make_unique<wxMenuGameFlowState>(host);const auto constructor=host.Snapshot(*object);
            auto& s=object->MenuStateForFixture();s.view40=view;s.mode44=mode;s.object4C=objectID?host.objects[objectID-1]:nullptr;s.allocation3C=bufferID?Token(40+bufferID):nullptr;object->MutableStateForAnalysis().field25=std::uint8_t(field25);
            if(op=="24")object->FlowSlot24ForAnalysis();else if(op=="28")object->FlowSlot28ForAnalysis();else if(op=="2C")object->FlowSlot2CForAnalysis();else Check(op=="ctor"||op=="destroy","Scoped Menu operation");
            const auto before=host.Snapshot(*object);object.reset();host.active=nullptr;
            std::cout<<"{\"events\":"<<Events(host)<<",\"constructor\":"<<constructor<<",\"beforeDestructor\":"<<before<<",\"unsubscribe\":"<<host.unsubscribe
                <<",\"alive\":{\"objects\":["<<bool(host.objectOwners[0])<<','<<bool(host.objectOwners[1])<<"],\"buffers\":["<<host.bufferAlive[0]<<','<<host.bufferAlive[1]
                <<"],\"timers\":["<<host.Alive(host.timerA)<<','<<host.Alive(host.timerB)<<"]}}\n";
        }
    }
    void TestLifetime()
    {
        Host host;host.fixedClock=true;wxMenuGameFlowState::SetFactoryHostForAnalysis(&host);
        auto source=spRTTIManager::Instance().Create(wxMenuGameFlowState::ClassID);auto target=spRTTIManager::Instance().Create(wxMenuGameFlowState::ClassID);
        wxMenuGameFlowState::SetFactoryHostForAnalysis(nullptr);
        auto* s=dynamic_cast<wxMenuGameFlowState*>(source.get());auto* t=dynamic_cast<wxMenuGameFlowState*>(target.get());Check(s&&t,"Typed Menu factory");
        Check(s->IsKindOf(wxGameFlowState::ClassID)&&s->GetMenuStateForAnalysis().timer50!=&s->TimerForAnalysis(),"Physical base with distinct own timer");
        const auto before=host.Snapshot(*t);s->MenuStateForFixture().view40=99;s->MenuStateForFixture().mode44=0xFFFFFFFF;s->MenuStateForFixture().word48=123;
        spCloneManager manager;Check(s->vfunc_14(*t,manager)&&host.Snapshot(*t)==before,"Copy preserves destination own data and timer");
        host.owner=2;auto clone=manager.Clone(*s);auto* c=dynamic_cast<wxMenuGameFlowState*>(clone.get());Check(c,"Typed clone");
        const auto& own=c->GetMenuStateForAnalysis();Check(own.view40==1&&own.mode44==1&&own.word48==0&&own.borrowedInput54==Token(2),"Clone fresh defaults and borrow");
        std::set<const spTaskTimer*> timers{&s->TimerForAnalysis(),s->GetMenuStateForAnalysis().timer50,&t->TimerForAnalysis(),t->GetMenuStateForAnalysis().timer50,&c->TimerForAnalysis(),own.timer50};Check(timers.size()==6,"Six distinct owned timers");
        Check(own.timer50->GetStateForAnalysis().active&&own.timer50->GetStateForAnalysis().startMilliseconds==1200,"Extra Reset then Start retains active state");
        clone.reset();target.reset();source.reset();host.active=nullptr;Check(host.reg==3&&host.unreg==3&&host.listeners.empty(),"All three lifetimes unregister");
        unsigned remaining=0;for(const auto& timer:host.timers)remaining+=bool(timer);Check(remaining==1&&host.Alive(host.timerB),"Only separate fixture timer retained");
    }
    void TestGuardsAndPlatformOrder()
    {
        Host host;wxMenuGameFlowState object(host);unsigned rejected=0;
        try{object.FlowSlot20ForAnalysis();}catch(const std::logic_error&){++rejected;}
        try{object.MenuSlot44ForAnalysis();}catch(const std::logic_error&){++rejected;}
        try{object.FlowSlot30ForAnalysis();}catch(const std::logic_error&){++rejected;}
        Check(rejected==3,"Unknown owned paths explicitly refuse");
        const auto before=host.Snapshot(object);object.vfunc_0C(nullptr);Check(host.Snapshot(object)==before,"Native Notify literal no-op");
        host.events.clear();host.flags=524288;object.MenuStateForFixture().mode44=1;object.FlowSlot2CForAnalysis();
        Check(std::find(host.events.begin(),host.events.end(),"M1:1")==host.events.end(),"PC snapshots mode after first input read");
        host.profile=Host::HookProfileForAnalysis::PS2;host.events.clear();object.MenuStateForFixture().mode44=1;object.FlowSlot2CForAnalysis();
        Check(std::find(host.events.begin(),host.events.end(),"M1:1")!=host.events.end(),"PS2 snapshots mode before first input read");
        host.flags=0;host.events.clear();object.FlowSlot28ForAnalysis();Check(host.events==std::vector<std::string>{"L1:1","W1:0","H1:0"},"PS2 moves flag from lookup to apply call");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--emit"){Emit(false);return 0;}
        if(argc==2&&std::string(argv[1])=="--emit-ps2"){Emit(true);return 0;}
        if(argc==2&&std::string(argv[1])=="--emit-lifetime")
        {
            TestLifetime();std::cout<<"{\"factoryObjects\":3,\"copyDestinationPreserved\":true,\"cloneFreshDefaultsAndBorrow\":true,\"distinctParentAndExtraTimers\":6,\"registrations\":3,\"unregistrations\":3,\"clonePairs\":1}\n";return 0;
        }
        TestLifetime();TestGuardsAndPlatformOrder();
        std::cout<<"PASS Menu bounded constructor/ownership/Copy/clone, hooks24/28/2C and explicit owned guards\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
