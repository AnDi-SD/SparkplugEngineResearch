#include "Code/wxMenuGameFlowState.h"
#include "Code/SparkBase/spNotification.h"
#include "Code/Sparkplug/spResource.h"
#include "Code/Sparkplug/spGameLevel.h"
#include "Code/Sparkplug/spEntity.h"
#include "Code/SparkplugPC/spPCProjectionManager.h"
#include <list>
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
    struct Host : wxMenuGameFlowStateHost
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
        void* FindMenuWindowPCForAnalysis(void* manager,std::uint32_t view) override{return Lookup(manager,view,"");}
        void* FindMenuWindowPS2ForAnalysis(void* manager,std::uint32_t view) override{return Lookup(manager,view,"");}
        void Window(void* p,std::string suffix){events.push_back("W"+std::to_string(Number(p)-30)+suffix);if(flags&16384)owner=2;}
        void ApplyMenuWindowPCForAnalysis(void* p,std::uint32_t flag) override{Window(p,':'+std::to_string(flag));}
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
    struct Slot30Host final : Host,wxGameFlowNavigationHost
    {
        std::array<std::array<char,64>,5> names{};
        unsigned button=1,hasWidget=1,flow=1,selected=0;
        std::uint32_t activeFlags=0,magnitude=0,angle=0;
        bool operation=false;
        Slot30Host()
        {
            std::strcpy(names[0].data(),"unknown");
            for(unsigned i=1;i<5;++i)std::strcpy(names[i].data(),("d"+std::to_string(i-1)).c_str());
        }
        static float Float(std::uint32_t word){float value;std::memcpy(&value,&word,4);return value;}
        unsigned WidgetID(void* p) const{const auto id=Number(p)-100;Check(id<5,"Known borrowed widget");return id;}
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override
        {if(operation)events.push_back("C2200");return {operation?2200u:1200u,1};}
        void* FindMenuWindowPCForAnalysis(void*,std::uint32_t view) override
        {events.push_back("L:"+std::to_string(view));return Token(31);}
        std::uint8_t ReadMenuInputByte1BForAnalysis(void* p) override
        {Check(p==Token(2),"Menu reads its own cached input54");return static_cast<std::uint8_t>(button);}
        void* ReadMenuActivatedWidget134ForAnalysis(void* p) override
        {Check(p==Token(31),"Menu reads current window134");return hasWidget?Token(100):nullptr;}
        const char* ReadMenuWidgetNameForAnalysis(void* p) override
        {events.push_back("K");return names[WidgetID(p)].data();}
        void* ResolveMenuFlowManagerForAnalysis() override{return Token(80+flow);}
        void InvokeMenuFlow5954C0ForAnalysis(void* p,std::uint32_t code,std::uint32_t value) override
        {
            Check(code==53&&value==0,"Exact first foreign flow command");
            events.push_back("S"+std::to_string(Number(p)-80)+':'+std::to_string(code)+':'+std::to_string(value));
            if(activeFlags&1)active->MenuStateForFixture().view40=7;
            if(activeFlags&4)std::strcpy(names[0].data(),"LoadSwamp_02");
            if(activeFlags&32)flow=2;
        }
        void InvokeMenuFlow595450ForAnalysis(void* p,std::uint32_t code,std::uint32_t value) override
        {
            Check(value==0,"Exact second foreign flow command argument");
            events.push_back("D"+std::to_string(Number(p)-80)+':'+std::to_string(code)+':'+std::to_string(value));
            if(activeFlags&2)active->MenuStateForFixture().view40=9;
            if(activeFlags&8)active->MutableStateForAnalysis().field25=0xEE;
        }
        wxGameFlowNavigationHost& MenuNavigationHostForAnalysis() override
        {events.push_back("N:"+std::to_string(active->GetMenuStateForAnalysis().view40));return *this;}
        float ReadNavigationMagnitudeForAnalysis(void* p) override
        {Check(p==Token(1),"Navigation uses distinct parent cached input30");return Float(magnitude);}
        float ReadNavigationAngleForAnalysis(void* p) override
        {Check(p==Token(1),"Navigation angle uses parent input30");return Float(angle);}
        void* ResolveNavigationWindowForAnalysis(std::uint32_t view) override
        {return FindMenuWindowPCForAnalysis(Token(21),view);}
        void* ReadInitialNavigationWidgetForAnalysis(void* p) override{return ReadMenuActivatedWidget134ForAnalysis(p);}
        void* ReadSelectedNavigationWidgetForAnalysis(void* p) override
        {Check(p==Token(31),"Borrowed selected widget window");return Token(100+selected);}
        const char* ReadNavigationLinkForAnalysis(void* p,unsigned direction) override
        {Check(direction<4,"Bounded direction");static constexpr const char* links[]{"d0","d1","d2","d3"};return WidgetID(p)==0?links[direction]:"empty";}
        void* FindNavigationWidgetForAnalysis(void* p,const char* name) override
        {
            Check(p==Token(31),"Borrowed lookup window");events.push_back(std::string("F:")+name);
            // Foreign lookup fixture follows its literal list order. These K
            // events describe its internal reader calls, not owned Menu logic.
            for(unsigned i=0;i<5;++i){events.push_back("K");if(std::strcmp(names[i].data(),name)==0)return Token(100+i);}
            return nullptr;
        }
        std::uint8_t ReadNavigationWidgetEnabledForAnalysis(void* p) override{(void)WidgetID(p);return 1;}
        void* ResolveNavigationNotificationReceiverForAnalysis() override{return nullptr;}
        void DispatchNotificationForAnalysis(void* p,const spNotificationForAnalysis& n) override{Host::DispatchNotificationForAnalysis(p,n);}
        void SelectNavigationWidgetForAnalysis(void* p,void* widget) override
        {Check(p==Token(31),"Borrowed selection window");selected=WidgetID(widget);events.push_back("P:"+std::to_string(selected));}
    };
    std::string TimerJSON(const spTaskTimer& timer)
    {
        const auto& s=timer.GetStateForAnalysis();std::uint32_t bits;std::memcpy(&bits,&s.deltaSeconds,4);
        std::ostringstream out;out<<'['<<s.active<<','<<s.relative<<','<<s.currentMilliseconds<<','<<s.startMilliseconds<<','<<s.pausedMilliseconds<<','<<bits<<']';return out.str();
    }
    void EmitSlot30()
    {
        std::string line;
        while(std::getline(std::cin,line))
        {
            if(line.empty())continue;Slot30Host host;std::istringstream in(line);std::string key;unsigned latch,ownTimer;
            Check(bool(in>>key>>latch>>host.button>>host.hasWidget>>ownTimer>>host.magnitude>>host.angle>>host.activeFlags),"Eight Slot30 input tokens");
            Check(key.size()<64&&latch<256&&host.button<256&&host.hasWidget<2&&ownTimer<3,"Slot30 fixture input bounds");
            std::strcpy(host.names[0].data(),key.c_str());auto object=std::make_unique<wxMenuGameFlowState>(host);
            auto* extra=object->MenuStateForFixture().timer50;object->MenuStateForFixture().view40=3;object->MenuStateForFixture().borrowedInput54=Token(2);object->MutableStateForAnalysis().field25=static_cast<std::uint8_t>(latch);
            if(ownTimer==0)object->MenuStateForFixture().timer50=nullptr;
            if(ownTimer==2){auto state=extra->GetStateForAnalysis();state.active=false;extra->SetStateForAnalysis(state);}
            host.events.clear();host.operation=true;const auto returned=object->FlowSlot30ForAnalysis();host.operation=false;
            const auto events=Events(host);const auto view=object->GetMenuStateForAnalysis().view40;const auto state=object->GetStateForAnalysis();
            const auto extraState=TimerJSON(*extra),parentState=TimerJSON(object->TimerForAnalysis());object.reset();host.active=nullptr;
            std::cout<<"{\"events\":"<<events<<",\"returned\":"<<returned<<",\"view\":"<<view<<",\"field25\":"<<unsigned(state.field25)<<",\"field34\":"<<state.field34<<",\"field38\":"<<unsigned(state.field38)<<",\"selected\":"<<host.selected
                <<",\"extraTimer\":"<<extraState<<",\"parentTimer\":"<<parentState<<",\"extraAliveAfterDestructor\":"<<(host.Alive(extra)?"true":"false")<<"}\n";
        }
    }
    void TestSlot30()
    {
        Slot30Host host;wxMenuGameFlowState object(host);object.MenuStateForFixture().borrowedInput54=Token(2);object.MenuStateForFixture().view40=3;
        host.operation=true;host.activeFlags=4;host.events.clear();Check(object.FlowSlot30ForAnalysis(),"Slot30 returns true");
        Check(std::find(host.events.begin(),host.events.end(),"D1:10:0")!=host.events.end(),"Borrowed name is compared after first foreign command");
        Check(object.GetStateForAnalysis().field25==0,"Activation clears the latch");
        host.button=0;Check(object.FlowSlot30ForAnalysis()&&object.GetStateForAnalysis().field25==1,"Released button rearms latch");
        host.profile=Host::HookProfileForAnalysis::PS2;bool rejected=false;try{object.FlowSlot30ForAnalysis();}catch(const std::logic_error&){rejected=true;}Check(rejected,"PS2 Slot30 explicitly refuses");
        host.operation=false;
    }
    // Observation fixtures call the actual shared manager/Entity bodies. They
    // describe controlled callback mutations, not recovered game policies.
    struct Entry44Host final : Host,spProjectionManagerHost,spEntityHost
    {
        struct Manager final : spPCProjectionManager
        {
            Entry44Host& host;unsigned id;
            Manager(Entry44Host& h,unsigned value):host(h),id(value){}
            bool InitializeProjectionsForAnalysis(spProjectionManagerHost& projectionHost) override
            {host.events.push_back("P"+std::to_string(id));return spPCProjectionManager::InitializeProjectionsForAnalysis(projectionHost);}
        };
        struct Entity final : spEntity
        {
            Entry44Host& host;unsigned role;
            Entity(Entry44Host& h,unsigned value):spEntity(h),host(h),role(value){}
            void vfunc_0C(const void* notification) noexcept override
            {host.Observe(role,notification);spEntity::vfunc_0C(notification);}
        };
        std::array<std::unique_ptr<Manager>,2> managers;
        std::array<std::unique_ptr<spGameLevel>,2> levels;
        std::array<spNode,2> roots;
        std::array<std::shared_ptr<spBaseObject>,4> entries;
        unsigned core=1,activeFlags=0;bool helperOwnerWritten=false,valid=true;
        Entry44Host()
        {
            for(unsigned i=0;i<2;++i)
            {
                managers[i]=std::make_unique<Manager>(*this,i+1);levels[i]=std::make_unique<spGameLevel>();
                roots[i].SetPositionForAnalysis(i?spNode::Vector3{7,8,9}:spNode::Vector3{1,2,3});
                roots[i].MarkLocalTransformDirtyForAnalysis();
            }
            entries[0]=std::make_shared<Entity>(*this,1);entries[1]=std::make_shared<Entity>(*this,2);
            entries[2]=std::make_shared<spNode>();entries[3]=std::make_shared<Entity>(*this,4);
        }
        void Seed(bool nonempty)
        {
            auto add=[this](unsigned level,std::list<std::shared_ptr<spBaseObject>> values)
            {
                auto instance=std::make_unique<spTemplateInstance>();instance->SetAttachedObjectsForFixture(std::move(values));
                Check(levels[level]->AddInstanceForAnalysis(std::move(instance)),"Established level/instance input");
            };
            if(nonempty){add(0,{entries[0],entries[2],entries[0]});add(0,{entries[1]});}
            add(1,{entries[3],entries[3]});
            if(activeFlags&1)managers[0]->AddProjectionForAnalysis(Token(900));
        }
        void Observe(unsigned role,const void* notification)
        {
            const auto& n=*static_cast<const spNotificationForAnalysis*>(notification);
            valid=valid&&n.code==0x1C&&n.word04==0&&n.word08==0&&n.word0C==0&&n.sender==nullptr&&n.word14==0&&(n.tail&255)==0;
            events.push_back("N"+std::to_string(role)+':'+std::to_string(n.payload));
            if(activeFlags&4){core=1;owner=2;active->MenuStateForFixture().view40=9;}
            if((activeFlags&128)&&n.payload==0)
                const_cast<spNotificationForAnalysis&>(n).tail|=1;
        }
        void* ResolveMenuEngineCoreForAnalysis() override{return Token(200+core);}
        void* ReadMenuScene18ForAnalysis(void* value) override
        {const auto id=Number(value)-200;Check(id>=1&&id<=2,"Core18 receiver");return Token(210+id);}
        spProjectionManager* ReadMenuProjectionManager2CForAnalysis(void* value) override
        {const auto id=Number(value)-211;Check(id<2,"Scene2C receiver");return managers[id].get();}
        spProjectionManagerHost& MenuProjectionHostForAnalysis() override{return *this;}
        spNode* ReadMenuSystemRoot14ForAnalysis(void* value) override
        {const auto id=Number(value)-211;Check(id<2,"Scene14 receiver");return &roots[id];}
        void* ReadProjectionHelper94ForAnalysis(void* value) override
        {Check(value==Token(900),"Projection94 receiver");return Token(901);}
        void WriteProjectionHelperOwner18ForAnalysis(void* helper,void* projection) override
        {Check(helper==Token(901)&&projection==Token(900),"Helper18 owner binding");helperOwnerWritten=true;}
        std::uint32_t InvokeProjectionHelper0ForAnalysis(void* value) override
        {
            Check(value==Token(901)&&helperOwnerWritten,"FXvirtual0 receiver");events.push_back("FX");
            if(activeFlags&2){core=2;active->MenuStateForFixture().object4C=levels[1].get();active->MenuStateForFixture().view40=7;}
            return 0;
        }
        std::uint32_t DrawProjection38ForAnalysis(void*,void*) override
        {throw std::logic_error("Entry44 does not call projection Draw38");}
        void SetMenuViewEnabledForAnalysis(void* value,std::uint32_t view,std::uint32_t enabled) override
        {
            events.push_back("V"+std::to_string(Number(value)-20)+':'+std::to_string(view)+':'+std::to_string(enabled));
            if(activeFlags&8){gui=2;active->MutableStateForAnalysis().field25=0xAA;}
        }
        void SetMenuHUDForAnalysis(void* value,std::uint32_t enabled) override
        {
            events.push_back("H"+std::to_string(Number(value)-20)+':'+std::to_string(enabled));
            if(activeFlags&16)active->MutableStateForAnalysis().field25=0xDD;
        }
        void SetMenuAudioEnabledForAnalysis(void* value,std::uint32_t enabled) override
        {
            Check(value==Token(62),"Entry audio receiver");events.push_back("E"+std::to_string(enabled));
            if(activeFlags&32)active->MutableStateForAnalysis().field25=0xCC;
        }
        std::uint8_t ReadMenuInputByte18ForAnalysis(void* value) override
        {
            const auto id=Number(value);Check(id>=1&&id<=2,"Entry input receiver");
            const auto result=deviceFlags[id-1];events.push_back("I"+std::to_string(id)+':'+std::to_string(result));
            if(activeFlags&64)sharedID=2;return result;
        }
        void AddToEngineManagerForAnalysis(spEntity&) override{}
        void DestroyEntityForAnalysis(spEntity&) noexcept override{}
        std::uint16_t GetReferenceCountForAnalysis(void*) override{throw std::logic_error("No Entity reference change");}
        void SetReferenceCountForAnalysis(void*,std::uint16_t) override{throw std::logic_error("No Entity reference change");}
        void DeleteReferenceForAnalysis(void*) override{throw std::logic_error("No Entity reference deletion");}
    };
    std::string NodeWords(const spNode& node)
    {
        std::ostringstream out;out<<'[';bool comma=false;
        auto append=[&](const auto& values){for(float value:values){std::uint32_t word;std::memcpy(&word,&value,4);if(comma)out<<',';comma=true;out<<word;}};
        append(node.GetWorldPositionForAnalysis());append(node.GetWorldScaleForAnalysis());append(node.GetWorldOrientationForAnalysis());
        return out.str()+','+std::to_string(node.GetFlagsForAnalysis())+']';
    }
    void EmitEntry44()
    {
        std::string line;
        while(std::getline(std::cin,line))
        {
            if(line.empty())continue;Entry44Host host;std::string op;unsigned nonempty,a,b;
            std::istringstream input(line);Check(bool(input>>op>>nonempty>>a>>b>>host.activeFlags),"Five Entry44 input tokens");
            Check((op=="20"||op=="44")&&nonempty<2&&a<256&&b<256&&host.activeFlags<256,"Entry44 input bounds");
            host.deviceFlags={std::uint8_t(a),std::uint8_t(b)};host.Seed(nonempty!=0);
            auto object=std::make_unique<wxMenuGameFlowState>(host);object->MenuStateForFixture().view40=3;
            object->MenuStateForFixture().object4C=host.levels[0].get();object->MutableStateForAnalysis().field25=0xA5;
            const auto parentTimer=TimerJSON(object->TimerForAnalysis()),ownTimer=TimerJSON(*object->GetMenuStateForAnalysis().timer50);
            std::array<long,4> references;for(unsigned i=0;i<4;++i)references[i]=host.entries[i].use_count();
            host.events.clear();bool returned=false;
            if(op=="44")returned=object->MenuSlot44ForAnalysis();else object->FlowSlot20ForAnalysis();
            bool sameRefs=true;for(unsigned i=0;i<4;++i)sameRefs=sameRefs&&references[i]==host.entries[i].use_count();
            Check(host.valid&&sameRefs,"Entry44 packet and borrowed references");
            const bool sameTimers=parentTimer==TimerJSON(object->TimerForAnalysis())&&ownTimer==TimerJSON(*object->GetMenuStateForAnalysis().timer50);
            Check(sameTimers,"Entry44 leaves both timers unchanged");
            std::cout<<"{\"events\":"<<Events(host)<<",\"returned\":"<<(op=="44"?(returned?"1":"0"):"null")
                <<",\"view\":"<<object->GetMenuStateForAnalysis().view40<<",\"field25\":"<<unsigned(object->GetStateForAnalysis().field25)
                <<",\"activeLevel\":"<<(object->GetMenuStateForAnalysis().object4C==host.levels[0].get()?1:2)
                <<",\"core\":"<<host.core<<",\"owner\":"<<host.owner<<",\"shared\":["<<unsigned(host.shared[0])<<','<<unsigned(host.shared[1])
                <<"],\"nodes\":["<<NodeWords(host.roots[0])<<','<<NodeWords(host.roots[1])<<"],\"projectionEnabled\":["
                <<unsigned(host.managers[0]->GetEnabledByteForAnalysis())<<','<<unsigned(host.managers[1]->GetEnabledByteForAnalysis())
                <<"],\"helperOwnerWritten\":"<<(host.helperOwnerWritten?"true":"false")<<",\"timersUnchanged\":true,\"borrowedReferencesUnchanged\":true}\n";
            // Same explicit retirement as the native harness: this operation
            // does not qualify nonempty owned GameLevel graph destruction.
            object->MenuStateForFixture().object4C=nullptr;object.reset();host.active=nullptr;
        }
    }
    void TestEntry44()
    {
        Entry44Host host;host.activeFlags=1|2|4|8|16|32|64|128;host.Seed(true);
        wxMenuGameFlowState object(host);object.MenuStateForFixture().object4C=host.levels[0].get();
        object.MenuStateForFixture().view40=3;object.MutableStateForAnalysis().field25=0xA5;
        host.deviceFlags={0,255};host.events.clear();
        Check(object.MenuSlot44ForAnalysis(),"Owned44 returns true after shared consumers");
        Check(host.events==std::vector<std::string>{"P1","FX","N4:0","N4:0","N4:1","N4:1"},"Projection callback replaces the level before collection");
        Check(host.valid&&host.core==1&&host.owner==2&&object.GetMenuStateForAnalysis().view40==9,"Notifications retain their global and view mutations");
        Check(host.roots[0].GetWorldPositionForAnalysis()==spNode::Vector3{1,2,3}&&host.roots[1].GetWorldPositionForAnalysis()==spNode::Vector3{0,0,0},"Root is freshly read after level notifications");
        Check(object.GetStateForAnalysis().field25==0xA5,"Owned44 leaves inherited latch unchanged");
        host.activeFlags=8|16|32|64;host.events.clear();object.FlowSlot20ForAnalysis();
        Check(host.events==std::vector<std::string>{"P1","FX","N4:0","N4:0","V1:9:1","I2:255","H2:0","A","E0"},"Entry uses fresh input and HUD-before-audio branch");
        Check(object.GetStateForAnalysis().field25==0xCC&&host.shared[0]==90&&host.shared[1]==0,"Entry preserves final audio mutation and fresh shared binding");
        host.profile=Host::HookProfileForAnalysis::PS2;unsigned refused=0;
        try{object.FlowSlot20ForAnalysis();}catch(const std::logic_error&){++refused;}
        try{object.MenuSlot44ForAnalysis();}catch(const std::logic_error&){++refused;}
        Check(refused==2,"PS2 Entry/owned44 explicitly refuse");
        object.MenuStateForFixture().object4C=nullptr;
    }
    struct PathHost final : Host
    {
        struct Buffer final{std::unique_ptr<char[]> owner;char* address;std::size_t size;unsigned id;bool freed=false;};
        std::vector<Buffer> buffers;
        std::array<char,256> input{};
        unsigned pathFlags=0,nextID=1;void* held=nullptr;bool operation=false,destruction=false,returnNull=false,hasInput=false,growOnAllocate=false;
        std::vector<std::string> destructorEvents;
        Buffer& Find(void* value)
        {for(auto& buffer:buffers)if(buffer.address==value)return buffer;throw std::logic_error("Known path allocation");}
        void* Create(std::size_t size,unsigned id)
        {
            auto data=std::make_unique<char[]>(size);std::memset(data.get(),0xCC,size);auto* address=data.get();
            buffers.push_back({std::move(data),address,size,id});return address;
        }
        void Seed(wxMenuGameFlowState& object,bool old)
        {
            if(old){auto* data=static_cast<char*>(Create(4,1));std::memcpy(data,"old",4);object.MenuStateForFixture().allocation3C=data;nextID=2;}
            if(pathFlags&4){held=Create(32,99);std::memcpy(held,"held",5);}
        }
        void* AllocateMenuPathBufferForAnalysis(std::size_t size) override
        {
            events.push_back("A"+std::to_string(size));
            if(pathFlags&8&&size>2){input[0]='x';input[1]=0;}
            else if(pathFlags&2&&size>1)input[0]='X';
            if(pathFlags&4){active->MenuStateForFixture().allocation3C=held;active->MutableStateForAnalysis().field25=0xAA;}
            if(growOnAllocate)std::memcpy(input.data(),"oversized",10);
            if(returnNull)return nullptr;
            return Create(size,nextID++);
        }
        void FreeMenuAllocationForAnalysis(void* value) override
        {
            auto& buffer=Find(value);Check(!buffer.freed,"Path allocation released once");
            (destruction?destructorEvents:events).push_back("F"+std::to_string(buffer.id));
            if(operation)
            {
                if((pathFlags&1)&&hasInput)std::memcpy(input.data(),"after-free",11);
                if(pathFlags&4){active->MenuStateForFixture().allocation3C=held;active->MutableStateForAnalysis().field25=0xDD;}
            }
            // Logical storage release; the fixture arena quarantines retired
            // bytes until fixture end, as the native monotonic arena does.
            buffer.freed=true;
        }
        static std::string Hex(const char* bytes,std::size_t size)
        {static constexpr char digits[]="0123456789abcdef";std::string result;for(std::size_t i=0;i<size;++i){const auto b=static_cast<unsigned char>(bytes[i]);result+=digits[b>>4];result+=digits[b&15];}return result;}
        const char* Decode(const std::string& token)
        {
            input.fill(char(0xCC));hasInput=token!="*";if(!hasInput)return nullptr;
            if(token=="-"){input[0]=0;return input.data();}
            Check(token.size()%2==0&&token.size()/2<128,"Bounded external byte-string input");
            auto nibble=[](char value)->unsigned{if(value>='0'&&value<='9')return value-'0';if(value>='a'&&value<='f')return value-'a'+10;throw std::logic_error("Hex path input");};
            for(std::size_t i=0;i<token.size()/2;++i)input[i]=char((nibble(token[2*i])<<4)|nibble(token[2*i+1]));
            input[token.size()/2]=0;return input.data();
        }
        std::string Snapshot(const wxMenuGameFlowState& object)
        {
            std::ostringstream out;auto* value=object.GetMenuStateForAnalysis().allocation3C;
            out<<"{\"events\":"<<Events(*this)<<",\"pathId\":"<<(value?Find(value).id:0)<<",\"capacity\":"<<(value?Find(value).size:0)<<",\"bytes\":";
            if(value){const auto& buffer=Find(value);out<<'"'<<Hex(buffer.address,buffer.size)<<'"';}else out<<"null";
            out<<",\"field25\":"<<unsigned(object.GetStateForAnalysis().field25)<<'}';return out.str();
        }
    };
    void EmitPath()
    {
        std::string line;
        while(std::getline(std::cin,line))
        {
            if(line.empty())continue;PathHost host;unsigned old;std::istringstream input(line);
            Check(bool(input>>old>>host.pathFlags)&&old<2&&host.pathFlags<16,"Path old-buffer/flags bounds");
            auto object=std::make_unique<wxMenuGameFlowState>(host);host.Seed(*object,old!=0);
            const auto before=object->GetMenuStateForAnalysis();const auto parent=object->GetStateForAnalysis();
            const auto parentTimer=TimerJSON(object->TimerForAnalysis()),ownTimer=TimerJSON(*before.timer50);
            Check(object->MenuSlot38ForAnalysis(),"Literal Slot38 before path replacements");
            std::vector<std::string> snapshots;std::string token;
            while(input>>token)
            {
                const auto* path=host.Decode(token);host.events.clear();host.operation=true;
                object->MenuSlot40ForAnalysis(path);host.operation=false;snapshots.push_back(host.Snapshot(*object));
            }
            Check(object->MenuSlot38ForAnalysis(),"Literal Slot38 after path replacements");
            const auto after=object->GetMenuStateForAnalysis();const auto state=object->GetStateForAnalysis();
            const bool unchanged=before.view40==after.view40&&before.mode44==after.mode44&&before.word48==after.word48&&before.object4C==after.object4C&&before.timer50==after.timer50&&before.borrowedInput54==after.borrowedInput54&&before.field58==after.field58
                &&parent.field10==state.field10&&parent.field14==state.field14&&parent.codes==state.codes&&parent.field24==state.field24&&parent.field34==state.field34&&parent.field38==state.field38;
            Check(unchanged&&parentTimer==TimerJSON(object->TimerForAnalysis())&&ownTimer==TimerJSON(*after.timer50),"Path replacement preserves other represented state and timers");
            const bool finalPath=after.allocation3C!=nullptr;host.destruction=true;object.reset();host.active=nullptr;
            std::ostringstream destructor;destructor<<'[';for(std::size_t i=0;i<host.destructorEvents.size();++i){if(i)destructor<<',';destructor<<'"'<<host.destructorEvents[i]<<'"';}destructor<<']';
            std::cout<<"{\"snapshots\":[";for(std::size_t i=0;i<snapshots.size();++i){if(i)std::cout<<',';std::cout<<snapshots[i];}
            std::cout<<"],\"slot38\":[1,1],\"otherRepresentedStateUnchanged\":true,\"timersUnchanged\":true,\"destructorEvents\":"<<destructor.str()
                <<",\"currentPathFreedByMenuDestructor\":"<<(finalPath?"true":"false")<<",\"oldPathFreed\":";
            bool oldFreed=false;if(old)for(const auto& buffer:host.buffers)if(buffer.id==1)oldFreed=buffer.freed;
            std::cout<<(old?(oldFreed?"true":"false"):"null")<<",\"callbackAllocationRetained\":"<<(host.held?(!host.Find(host.held).freed?"true":"false"):"null")<<"}\n";
        }
    }
    void TestPath()
    {
        PathHost host;wxMenuGameFlowState object(host);host.pathFlags=1|2|4|8;host.Seed(object,true);
        const auto* input=host.Decode("47617264656e69612f6c6f6e672d6e616d65");host.events.clear();host.operation=true;
        object.MenuSlot40ForAnalysis(input);host.operation=false;
        auto& buffer=host.Find(object.GetMenuStateForAnalysis().allocation3C);
        Check(buffer.size==11&&buffer.address[0]=='x'&&buffer.address[1]==0&&static_cast<unsigned char>(buffer.address[2])==0xCC,"Free mutation precedes length; allocation mutation precedes byte-copy");
        Check(host.events==std::vector<std::string>{"F1","A11"}&&object.GetStateForAnalysis().field25==0xAA,"Path storage callback effects survive publication");
        host.returnNull=true;host.pathFlags=4;input=host.Decode("616c706861");host.operation=true;bool refused=false;
        try{object.MenuSlot40ForAnalysis(input);}catch(const std::logic_error&){refused=true;}host.operation=false;
        Check(refused&&object.GetMenuStateForAnalysis().allocation3C==nullptr,"Null storage publishes null then refuses; no native failure claim");
        host.returnNull=false;host.pathFlags=0;host.growOnAllocate=true;input=host.Decode("61");host.operation=true;refused=false;
        try{object.MenuSlot40ForAnalysis(input);}catch(const std::logic_error&){refused=true;}host.operation=false;
        auto& partial=host.Find(object.GetMenuStateForAnalysis().allocation3C);
        Check(refused&&partial.size==2&&partial.address[0]=='o'&&partial.address[1]=='v',"Growing input preserves a bounded copied prefix then refuses");
        host.profile=Host::HookProfileForAnalysis::PS2;unsigned ps2=0;
        try{object.MenuSlot40ForAnalysis(nullptr);}catch(const std::logic_error&){++ps2;}
        try{object.MenuSlot38ForAnalysis();}catch(const std::logic_error&){++ps2;}
        Check(ps2==2,"PS2 Slot40/38 explicitly refuse");
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
        Check(rejected==3,"Unbound required providers explicitly refuse");
        const auto before=host.Snapshot(object);object.vfunc_0C(nullptr);Check(host.Snapshot(object)==before,"Native Notify literal no-op");
        host.events.clear();host.flags=524288;object.MenuStateForFixture().mode44=1;object.FlowSlot2CForAnalysis();
        Check(std::find(host.events.begin(),host.events.end(),"M1:1")==host.events.end(),"PC snapshots mode after first input read");
        host.profile=Host::HookProfileForAnalysis::PS2;host.events.clear();object.MenuStateForFixture().mode44=1;object.FlowSlot2CForAnalysis();
        Check(std::find(host.events.begin(),host.events.end(),"M1:1")!=host.events.end(),"PS2 snapshots mode before first input read");
        host.flags=0;host.events.clear();object.FlowSlot28ForAnalysis();Check(host.events==std::vector<std::string>{"L1:1","W1:0","H1:0"},"PS2 passes flag to apply call");
        host.profile=Host::HookProfileForAnalysis::PC;host.events.clear();object.FlowSlot28ForAnalysis();Check(host.events==std::vector<std::string>{"L1:1","W1:0","H1:0"},"PC lookup consumes view; apply consumes flag");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--emit"){Emit(false);return 0;}
        if(argc==2&&std::string(argv[1])=="--emit-ps2"){Emit(true);return 0;}
        if(argc==2&&std::string(argv[1])=="--emit-slot30"){EmitSlot30();return 0;}
        if(argc==2&&std::string(argv[1])=="--emit-entry44"){EmitEntry44();return 0;}
        if(argc==2&&std::string(argv[1])=="--emit-path"){EmitPath();return 0;}
        if(argc==2&&std::string(argv[1])=="--emit-lifetime")
        {
            TestLifetime();std::cout<<"{\"factoryObjects\":3,\"copyDestinationPreserved\":true,\"cloneFreshDefaultsAndBorrow\":true,\"distinctParentAndExtraTimers\":6,\"registrations\":3,\"unregistrations\":3,\"clonePairs\":1}\n";return 0;
        }
        TestLifetime();TestGuardsAndPlatformOrder();TestSlot30();TestEntry44();TestPath();
        std::cout<<"PASS Menu bounded lifetime, hooks24/28/2C, PC Slot30/navigation, Entry/owned44 shared consumers, path/literal leaves and explicit guards\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
