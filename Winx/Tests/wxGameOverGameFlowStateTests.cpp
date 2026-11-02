#include "Code/wxGameOverGameFlowState.h"
#include "Analysis/PC/wxGameOverGameFlowStateAbi.h"
#include "Code/SparkBase/spNotification.h"
#include "Code/Sparkplug/spGameLevel.h"
#include "Code/Sparkplug/spTemplateInstance.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/SparkplugPC/spPCProjectionManager.h"
#include "Analysis/Host/spProjectionManagerHost.h"
#include <sstream>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    std::array<const void*,3> watched{};
    std::array<unsigned,3> freed{};
    bool watching=false;
    const void* entryWatchedLevel=nullptr;unsigned entryFreedLevel=0;
    void Released(void* address) noexcept
    {if(watching){for(unsigned i=0;i<watched.size();++i)if(watched[i]==address)++freed[i];if(entryWatchedLevel&&entryWatchedLevel==address)++entryFreedLevel;}}
}
void* operator new(std::size_t size){if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{Released(p);std::free(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}

namespace
{
    void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
    template<class F>void Refuses(F&& operation)
    {bool refused=false;try{operation();}catch(const std::logic_error&){refused=true;}Check(refused,"Expected explicit analytical refusal");}
    using Input=std::array<std::uint8_t,0x30>;
    using Read=std::array<unsigned,4>;
    using Effect=std::array<unsigned,4>;
    struct Config{unsigned index=0,flag=0,a1B=0,a2A=0,b1B=0,b2A=0,mutation=0,afterRead=0,flowMutation=0,resolveMutation=0;};
    std::array<std::uint32_t,6> TimerState(const spTaskTimer& timer)
    {
        const auto& value=timer.GetStateForAnalysis();std::uint32_t bits;
        std::memcpy(&bits,&value.deltaSeconds,sizeof(bits));
        return {value.active,value.relative,value.currentMilliseconds,value.startMilliseconds,value.pausedMilliseconds,bits};
    }
    bool SameExcept25(const wxGameFlowState::StateForAnalysis& a,const wxGameFlowState::StateForAnalysis& b)
    {return a.field10==b.field10&&a.field14==b.field14&&a.codes==b.codes&&a.field24==b.field24&&a.field34==b.field34&&a.field38==b.field38;}
    struct Host:wxGameOverGameFlowStateHost
    {
        std::array<Input,2> inputs{};
        std::array<std::array<std::byte,8>,2> flows{};
        wxGameFlowResourcesForAnalysis resources;
        std::unique_ptr<spTaskTimer> menuTimer;
        wxMenuGameFlowState* registered=nullptr;
        wxGameOverGameFlowState* active=nullptr;
        Config config;
        std::vector<Read> reads;
        std::vector<Effect> effects;
        std::vector<unsigned> borrows;
        unsigned owner=1,flowID=1,clockCalls=0,registerCalls=0,unregisterCalls=0,unqualifiedParentCalls=0;
        bool constructorMutation=false,qualified=false,missingFlow=false;
        HookProfileForAnalysis platform=HookProfileForAnalysis::PC;
        Host(){reads.reserve(4);effects.reserve(1);borrows.reserve(3);}
        unsigned InputID(const void* p) const
        {if(!p)return 0;for(unsigned i=0;i<inputs.size();++i)if(p==inputs[i].data())return i+1;throw std::runtime_error("Unknown borrowed input");}
        unsigned FlowID(const void* p) const
        {for(unsigned i=0;i<flows.size();++i)if(p==flows[i].data())return i+1;throw std::runtime_error("Unknown foreign flow");}
        void* BorrowInputForAnalysis() override
        {if(constructorMutation&&borrows.size()==2)owner=2;borrows.push_back(owner);return inputs[owner-1].data();}
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override{++clockCalls;return {1200,1};}
        wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() override{return resources;}
        HookProfileForAnalysis MenuHookProfileForAnalysis() const noexcept override{return platform;}
        TickProfileForAnalysis GameOverTickProfileForAnalysis() const noexcept override
        {return qualified?TickProfileForAnalysis::PCStableBorrowedInputAndFlow:TickProfileForAnalysis::Unqualified;}
        void RegisterMenuReceiverForAnalysis(wxMenuGameFlowState& value,std::uint32_t tag) override
        {Check(!registered&&tag==0x20,"Exact parent subscription");registered=&value;++registerCalls;}
        void UnregisterMenuReceiverForAnalysis(wxMenuGameFlowState& value,std::uint32_t tag) override
        {Check(registered==&value&&tag==0x20,"Exact parent unsubscription");registered=nullptr;++unregisterCalls;}
        void StoreMenuTimerAllocationForAnalysis(std::unique_ptr<spTaskTimer> value) override
        {Check(!menuTimer,"One real Menu timer");menuTimer=std::move(value);}
        std::unique_ptr<spTaskTimer> TakeMenuTimerAllocationForAnalysis(spTaskTimer& value) override
        {Check(menuTimer.get()==&value,"Take actual Menu timer owner");return std::move(menuTimer);}
        std::uint8_t ReadByte(void* pointer,unsigned offset)
        {
            Check(active,"Live own receiver during observation");const auto id=InputID(pointer);Check(id!=0,"Live input observation");
            const auto value=inputs[id-1][offset];
            reads.push_back({id,offset,value,InputID(active->GetGameOverStateForAnalysis().borrowedInput5C)});
            // The value is captured before this explicit input mutation. The
            // class must retain its captured pointer for the rest of this pair.
            if(config.afterRead==reads.size())
            {
                if(config.mutation==1)inputs[0][0x1B]=1;
                else if(config.mutation==2)inputs[0][0x2A]=1;
                else if(config.mutation==3)active->GameOverStateForFixture().borrowedInput5C=inputs[1].data();
            }
            return value;
        }
        std::uint8_t ReadMenuInputByte1BForAnalysis(void* p) override{return ReadByte(p,0x1B);}
        std::uint8_t ReadGameOverInputByte2AForAnalysis(void* p) override{return ReadByte(p,0x2A);}
        void* ResolveMenuFlowManagerForAnalysis() override
        {
            if(missingFlow)return nullptr;
            auto* captured=flows[flowID-1].data();
            if(config.resolveMutation)
            {flowID=2;active->MutableStateForAnalysis().field25=0xEE;active->GameOverStateForFixture().borrowedInput5C=inputs[1].data();}
            return captured;
        }
        void InvokeMenuFlow595450ForAnalysis(void* receiver,std::uint32_t state,std::uint32_t argument) override
        {
            Check(active&&FlowID(receiver)==1&&state==0x38&&argument==0,"Captured foreign flow receiver/arguments");
            const auto flag=active->GetStateForAnalysis().field25;
            Check(flag==(config.resolveMutation?0xEE:0),"Own clear precedes flow resolution and foreign invocation");
            effects.push_back({FlowID(receiver),state,argument,flag});
            if(config.flowMutation)
            {active->MutableStateForAnalysis().field25=0xA5;active->GameOverStateForFixture().borrowedInput5C=inputs[1].data();owner=2;}
        }
        // Unused parent hook dependencies are failures, never successful
        // replacements for unknown active methods in this bounded suite.
        std::unique_ptr<spBaseObject> TakeMenuObjectAllocationForAnalysis(spBaseObject&) override{throw std::logic_error("Unexpected owned Menu object");}
        void FreeMenuAllocationForAnalysis(void*) override{throw std::logic_error("Unexpected Menu buffer");}
        void* AllocateMenuPathBufferForAnalysis(std::size_t) override{++unqualifiedParentCalls;throw std::logic_error("Unexpected base Menu path operation");}
        void* ResolveMenuEngineCoreForAnalysis() override{++unqualifiedParentCalls;throw std::logic_error("Unexpected base Menu projection operation");}
        void DispatchNotificationForAnalysis(void*,const spNotificationForAnalysis&) override{throw std::logic_error("Unexpected notification");}
        void* ResolveMenuGameContextForAnalysis() override{throw std::logic_error("Unexpected game context");}
        void* ReadMenuNotificationTarget2B4ForAnalysis(void*) override{throw std::logic_error("Unexpected target");}
        void* ResolveMenuGUIManagerForAnalysis() override{throw std::logic_error("Unexpected GUI");}
        void SetMenuViewEnabledForAnalysis(void*,std::uint32_t,std::uint32_t) override{throw std::logic_error("Unexpected view");}
        void* FindMenuWindowPCForAnalysis(void*,std::uint32_t) override{throw std::logic_error("Unexpected window");}
        void ApplyMenuWindowPCForAnalysis(void*,std::uint32_t) override{throw std::logic_error("Unexpected window");}
        void* FindMenuWindowPS2ForAnalysis(void*,std::uint32_t) override{throw std::logic_error("Unexpected PS2 window");}
        void ApplyMenuWindowPS2ForAnalysis(void*,std::uint32_t) override{throw std::logic_error("Unexpected PS2 window");}
        void SetMenuHUDForAnalysis(void*,std::uint32_t) override{throw std::logic_error("Unexpected HUD");}
        void* ResolveMenuAudioInterfaceForAnalysis() override{throw std::logic_error("Unexpected legacy input view");}
        void* GetMenuAudioControllerForAnalysis(void*) override{throw std::logic_error("Unexpected legacy input device");}
        void SetMenuAudioEnabledForAnalysis(void*,std::uint32_t) override{throw std::logic_error("Unexpected input device operation");}
        void* ResolveMenuExitServiceForAnalysis() override{throw std::logic_error("Unexpected Exit service");}
        void InvokeMenuExitServiceForAnalysis(void*,std::uint32_t) override{throw std::logic_error("Unexpected Exit service");}
        void* ResolveMenuInputOwnerForAnalysis() override{throw std::logic_error("Unexpected active input-owner resolution");}
        void* ReadMenuInput1CForAnalysis(void*) override{throw std::logic_error("Unexpected input1C");}
        void* ReadMenuInput18ForAnalysis(void*) override{throw std::logic_error("Unexpected input18");}
        std::uint8_t ReadMenuInputByte18ForAnalysis(void*) override{throw std::logic_error("Unexpected input18 flag");}
        void ResetMenuInputForAnalysis(void*) override{throw std::logic_error("Unexpected input Reset");}
        void SetMenuInputModeForAnalysis(void*,std::uint32_t) override{throw std::logic_error("Unexpected input mode");}
        std::uint8_t& BorrowMenuSharedFlag45ForAnalysis() override{throw std::logic_error("Unexpected shared45");}
    };
    void Arm(const wxGameOverGameFlowState& value)
    {
        watched={&value,&value.TimerForAnalysis(),value.GetMenuStateForAnalysis().timer50};freed={};
        Check(watched[0]!=watched[1]&&watched[0]!=watched[2]&&watched[1]!=watched[2],"Three distinct real owned allocations");watching=true;
    }
    void Closed(const Host& host)
    {
        watching=false;Check(freed==std::array<unsigned,3>{1,1,1},"Actual object and both real parent timer allocations released once");
        Check(!host.menuTimer&&!host.registered&&host.registerCalls==1&&host.unregisterCalls==1,"Real parent lifetime/storage ownership");
    }
    void Lifecycle(bool emit)
    {
        Host host;host.constructorMutation=true;auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();
        Check(object->IsExactly(wxGameOverGameFlowState::ClassID)&&object->IsKindOf(wxMenuGameFlowState::ClassID)&&object->IsKindOf(spBaseObject::ClassID),"Typed physical parent and engine RTTI");
        Check(!wxGameOverGameFlowState::StaticRTTI().factory,"No implicit unbound factory");
        Check(host.borrows==std::vector<unsigned>{1,1,2},"Own constructor obtains fresh input after parent observations");
        const auto view=object->GetMenuStateForAnalysis().view40;
        const auto baseID=host.InputID(object->GetBorrowedInputForAnalysis());
        const auto menuID=host.InputID(object->GetMenuStateForAnalysis().borrowedInput54);
        const auto ownID=host.InputID(object->GetGameOverStateForAnalysis().borrowedInput5C);
        Check(view==32&&baseID==1&&menuID==1&&ownID==2,"Known view and three independent input bindings");
        auto before=host.inputs;Arm(*object);host.active=nullptr;object.reset();Closed(host);Check(host.inputs==before,"Borrowed inputs survive actual source cleanup");
        if(emit)std::cout<<'['<<view<<','<<baseID<<','<<menuID<<','<<ownID<<",3,1]\n";
    }
    template<std::size_t N>void EmitArray(const std::array<unsigned,N>& values)
    {std::cout<<'[';for(std::size_t i=0;i<N;++i){if(i)std::cout<<',';std::cout<<values[i];}std::cout<<']';}
    void One(const Config& config,bool emit)
    {
        Check(config.flag<=255&&config.a1B<=255&&config.a2A<=255&&config.b1B<=255&&config.b2A<=255&&config.mutation<=3,"Bounded raw inputs");
        Host host;host.qualified=true;host.config=config;
        auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();
        host.inputs[0][0x1B]=static_cast<std::uint8_t>(config.a1B);host.inputs[0][0x2A]=static_cast<std::uint8_t>(config.a2A);
        host.inputs[1][0x1B]=static_cast<std::uint8_t>(config.b1B);host.inputs[1][0x2A]=static_cast<std::uint8_t>(config.b2A);
        object->MutableStateForAnalysis().field25=static_cast<std::uint8_t>(config.flag);
        const auto parent=object->GetStateForAnalysis();const auto menu=object->GetMenuStateForAnalysis();
        const auto taskTimer=TimerState(object->TimerForAnalysis()),menuTimer=TimerState(*menu.timer50);
        const auto clocks=host.clockCalls;const auto inputBefore=host.inputs;
        const bool returned=object->FlowSlot30ForAnalysis();
        Check(returned&&SameExcept25(parent,object->GetStateForAnalysis()),"Own Tick preserves other typed GameFlow fields");
        const auto& afterMenu=object->GetMenuStateForAnalysis();
        Check(afterMenu.allocation3C==menu.allocation3C&&afterMenu.view40==menu.view40&&afterMenu.mode44==menu.mode44&&afterMenu.word48==menu.word48
            &&afterMenu.object4C==menu.object4C&&afterMenu.timer50==menu.timer50&&afterMenu.borrowedInput54==menu.borrowedInput54&&afterMenu.field58==menu.field58,"Own Tick preserves typed Menu state");
        Check(taskTimer==TimerState(object->TimerForAnalysis())&&menuTimer==TimerState(*menu.timer50)&&clocks==host.clockCalls,"No parent timer/clock operation during own Tick");
        auto expectedInputs=inputBefore;
        if(config.afterRead&&config.afterRead<=host.reads.size())
        {if(config.mutation==1)expectedInputs[0][0x1B]=1;else if(config.mutation==2)expectedInputs[0][0x2A]=1;}
        Check(host.inputs==expectedInputs,"Only declared borrowed-input mutation");
        std::array<unsigned,6> state{object->GetStateForAnalysis().field25,host.InputID(object->GetGameOverStateForAnalysis().borrowedInput5C),host.flowID,host.owner,static_cast<unsigned>(host.reads.size()),static_cast<unsigned>(host.effects.size())};
        const auto afterInputs=host.inputs;Arm(*object);host.active=nullptr;object.reset();Closed(host);
        Check(host.inputs==afterInputs,"No borrowed-input release or trailing cleanup effect");
        if(emit)
        {
            std::cout<<"{\"state\":";EmitArray(state);std::cout<<",\"returned\":"<<returned<<",\"reads\":[";
            for(std::size_t i=0;i<host.reads.size();++i){if(i)std::cout<<',';EmitArray(host.reads[i]);}
            std::cout<<"],\"flow\":[";for(std::size_t i=0;i<host.effects.size();++i){if(i)std::cout<<',';EmitArray(host.effects[i]);}std::cout<<"]}\n";
        }
    }
    struct EntryConfig {unsigned index=0,parentInput18=0,ownInput18=0,mutations=0;std::uint32_t rawMode44=1;unsigned cachedInputID=1;};
    struct EntryHost final:Host,spProjectionManagerHost
    {
        struct Event {std::string part,kind;unsigned receiver;std::vector<unsigned> arguments;std::string before,after;};
        struct Capture {unsigned owner,hook;std::uint32_t mode;};
        EntryConfig entry;
        std::unique_ptr<spGameLevel> level=std::make_unique<spGameLevel>();
        spPCProjectionManager projection;
        spNode root;
        std::array<std::array<std::byte,8>,2> guiBindings{},dxBindings{},deviceBindings{},ownerBindings{};
        std::array<std::array<std::byte,8>,4> hookBindings{};
        std::array<unsigned,2> ownerMembers{1,3};
        std::array<std::uint8_t,2> ownerFlags{0xA5,0x5A},sharedFlags{0x5A,0xA5};
        unsigned gui=1,shared=1,dx=1,parentEvents=0;
        bool admitted=true,parent=true,pendingParentReturn=false,missingSecondMember=false;
        std::vector<Event> events;
        std::vector<Capture> captures;
        explicit EntryHost(const EntryConfig& config):entry(config)
        {
            root.SetPositionForAnalysis({1,2,3});root.SetOrientationForAnalysis(spNode::Matrix3{});root.MarkLocalTransformDirtyForAnalysis();
            inputs[0][0x18]=static_cast<std::uint8_t>(entry.parentInput18);inputs[1][0x18]=static_cast<std::uint8_t>(entry.ownInput18);
            events.reserve(9);captures.reserve(1);
        }
        static unsigned Identity(const void* value,const auto& bindings)
        {for(unsigned i=0;i<bindings.size();++i)if(value==bindings[i].data())return i+1;throw std::runtime_error("Unknown Entry borrowed binding");}
        static void Numbers(std::ostream& out,const auto& values)
        {out<<'[';bool comma=false;for(auto value:values){if(comma)out<<',';comma=true;out<<static_cast<std::uint32_t>(value);}out<<']';}
        std::vector<std::uint32_t> RootWords() const
        {
            std::vector<std::uint32_t> result;
            const auto append=[&](const auto& values){for(float value:values){std::uint32_t bits;std::memcpy(&bits,&value,4);result.push_back(bits);}};
            append(root.GetWorldPositionForAnalysis());append(root.GetWorldScaleForAnalysis());append(root.GetWorldOrientationForAnalysis());result.push_back(root.GetFlagsForAnalysis());return result;
        }
        std::string Snapshot() const
        {
            const auto& menu=active->GetMenuStateForAnalysis();std::ostringstream out;
            out<<"{\"view\":"<<menu.view40<<",\"mode\":"<<menu.mode44<<",\"field25\":"<<unsigned(active->GetStateForAnalysis().field25)
                <<",\"cachedInputID\":"<<InputID(active->GetGameOverStateForAnalysis().borrowedInput5C)<<",\"shared45\":";
            Numbers(out,sharedFlags);out<<",\"globalGUI\":"<<gui<<",\"globalShared\":"<<shared<<",\"globalDX\":"<<dx<<",\"globalOwner\":"<<owner<<",\"ownerMembers1C\":";
            Numbers(out,ownerMembers);out<<",\"ownerFlags20\":";Numbers(out,ownerFlags);out<<",\"rootWords\":";Numbers(out,RootWords());
            out<<",\"projectionEnabled\":"<<unsigned(projection.GetEnabledByteForAnalysis())<<'}';return out.str();
        }
        void Globals2(){gui=shared=dx=owner=2;}
        Event Begin(const char* kind,unsigned receiver,std::vector<unsigned> arguments={})
        {return {parent?"parent":"own",kind,receiver,std::move(arguments),Snapshot(),{}};}
        void End(Event event)
        {
            event.after=Snapshot();events.push_back(std::move(event));
            if(parent&&++parentEvents==4)pendingParentReturn=true;
        }
        void ParentTail()
        {if(parent&&parentEvents==3&&(entry.mutations&128)){Globals2();active->MutableStateForAnalysis().field25=0xEE;}}
        EntryProfileForAnalysis GameOverEntryProfileForAnalysis() const noexcept override
        {return admitted?EntryProfileForAnalysis::PCEmptyOwnedGameLevel:EntryProfileForAnalysis::Unqualified;}
        std::unique_ptr<spBaseObject> TakeMenuObjectAllocationForAnalysis(spBaseObject& value) override
        {Check(&value==level.get(),"Entry retires its actual owned GameLevel");return std::move(level);}
        void* ResolveMenuEngineCoreForAnalysis() override{return &root;}
        void* ReadMenuScene18ForAnalysis(void* value) override{Check(value==&root,"Entry Core18 binding");return &projection;}
        spProjectionManager* ReadMenuProjectionManager2CForAnalysis(void* value) override{Check(value==&projection,"Entry Scene2C");return &projection;}
        spNode* ReadMenuSystemRoot14ForAnalysis(void* value) override{Check(value==&projection,"Entry Scene14");return &root;}
        spProjectionManagerHost& MenuProjectionHostForAnalysis() override{return *this;}
        void* ReadProjectionHelper94ForAnalysis(void*) override{throw std::logic_error("Unexpected nonempty projection helper");}
        void WriteProjectionHelperOwner18ForAnalysis(void*,void*) override{throw std::logic_error("Unexpected projection owner");}
        std::uint32_t InvokeProjectionHelper0ForAnalysis(void*) override{throw std::logic_error("Unexpected projection invocation");}
        std::uint32_t DrawProjection38ForAnalysis(void*,void*) override{throw std::logic_error("Unexpected projection drawing");}
        void* ResolveMenuGUIManagerForAnalysis() override{return guiBindings[gui-1].data();}
        void SetMenuViewEnabledForAnalysis(void* value,std::uint32_t view,std::uint32_t enabled) override
        {Check(view==0x20&&enabled==1,"Entry literal view arguments");auto event=Begin("GUIView",Identity(value,guiBindings),{view,enabled});End(std::move(event));}
        void SetMenuHUDForAnalysis(void* value,std::uint32_t enabled) override
        {
            Check(enabled==unsigned(!(parent?entry.parentInput18:entry.ownInput18)),"Entry conditional HUD argument");auto event=Begin("GUIHUD",Identity(value,guiBindings),{enabled});
            if(!parent&&enabled==0&&(entry.mutations&2)){Globals2();active->MutableStateForAnalysis().field25=0xA5;}
            ParentTail();End(std::move(event));
        }
        void* ResolveMenuAudioInterfaceForAnalysis() override{return dxBindings[dx-1].data();}
        void* GetMenuAudioControllerForAnalysis(void* value) override
        {const auto id=Identity(value,dxBindings);auto event=Begin("DXView18Getter0C",id);End(std::move(event));return deviceBindings[id-1].data();}
        void SetMenuAudioEnabledForAnalysis(void* value,std::uint32_t enabled) override
        {
            Check(enabled==unsigned(!(parent?entry.parentInput18:entry.ownInput18)),"Entry conditional DX argument");auto event=Begin("DXDevice20",Identity(value,deviceBindings),{enabled});
            if(!parent&&enabled==1&&(entry.mutations&4)){Globals2();active->MutableStateForAnalysis().field25=0xA5;}
            ParentTail();End(std::move(event));
        }
        void* ResolveMenuInputOwnerForAnalysis() override
        {
            if(pendingParentReturn)
            {
                // The next existing observation applies the supplied native
                // parent-return input. No extra parent/store callback exists.
                pendingParentReturn=false;parent=false;if(entry.mutations&1)owner=2;
                inputs[owner-1][0x18]=static_cast<std::uint8_t>(entry.ownInput18);
            }
            return ownerBindings[owner-1].data();
        }
        void* ReadMenuInput18ForAnalysis(void* value) override{return inputs[Identity(value,ownerBindings)-1].data();}
        std::uint8_t ReadMenuInputByte18ForAnalysis(void* value) override{return inputs[InputID(value)-1][0x18];}
        std::uint8_t& BorrowMenuSharedFlag45ForAnalysis() override{return sharedFlags[shared-1];}
        void* ReadMenuInput1CForAnalysis(void* value) override
        {if(missingSecondMember&&!captures.empty())return nullptr;return hookBindings[ownerMembers[Identity(value,ownerBindings)-1]-1].data();}
        void ResetMenuInputForAnalysis(void* value) override
        {
            const auto hook=Identity(value,hookBindings),captured=(hook-1)/2;Check(hook==2*captured+1,"Entry Reset initial member binding");
            auto event=Begin("UserInputReset20",hook);captures.push_back({captured+1,hook,active->GetMenuStateForAnalysis().mode44});
            if(entry.mutations&8)ownerMembers[captured]=2*captured+2;
            if(entry.mutations&16)owner=2-captured;
            if(entry.mutations&32){active->MenuStateForFixture().mode44=0xFFFFFFFF;active->MutableStateForAnalysis().field25=0xA5;}
            End(std::move(event));
        }
        void SetMenuInputModeForAnalysis(void* value,std::uint32_t mode) override
        {
            Check(!captures.empty()&&Identity(value,hookBindings)==ownerMembers[captures.back().owner-1]&&mode==0,"Entry cached-owner fresh member/literal zero mode");
            auto event=Begin("UserInputSetMode1C",Identity(value,hookBindings),{mode});
            if(entry.mutations&64){owner=3-owner;ownerFlags[owner-1]=0xEE;active->MutableStateForAnalysis().field25=0xA5;}
            End(std::move(event));
        }
        std::uint8_t& BorrowGameOverInputOwnerFlag20ForAnalysis(void* value) override{return ownerFlags[Identity(value,ownerBindings)-1];}
    };
    void EntryOne(const EntryConfig& config,bool emit)
    {
        Check(config.parentInput18<=255&&config.ownInput18<=255&&config.mutations<256&&config.cachedInputID<=1,"Entry bounded inputs");
        EntryHost host(config);auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();
        object->MenuStateForFixture().object4C=host.level.get();object->MenuStateForFixture().mode44=config.rawMode44;
        object->GameOverStateForFixture().borrowedInput5C=config.cachedInputID?host.inputs[0].data():nullptr;
        const auto before=object->GetStateForAnalysis();const auto menuBefore=object->GetMenuStateForAnalysis();
        const auto timerBefore=TimerState(object->TimerForAnalysis()),menuTimerBefore=TimerState(*menuBefore.timer50);const auto clocks=host.clockCalls;
        object->FlowSlot20ForAnalysis();
        Check(SameExcept25(before,object->GetStateForAnalysis())&&object->GetStateForAnalysis().field25==0,"Entry preserves other typed Flow fields/final25 clear");
        Check(host.events.size()==9&&host.captures.size()==1&&!host.pendingParentReturn,"Whole real Entry foreign observations");
        Check(timerBefore==TimerState(object->TimerForAnalysis())&&menuTimerBefore==TimerState(*menuBefore.timer50)&&clocks==host.clockCalls,"Entry parent timers/clock unchanged");
        Check(object->GetMenuStateForAnalysis().object4C==host.level.get()&&host.level->GetName()==nullptr&&host.level->GetInstanceCountForAnalysis()==0,"Empty owned GameLevel retained through Entry");
        Check(object->GetMenuStateForAnalysis().allocation3C==menuBefore.allocation3C&&object->GetMenuStateForAnalysis().view40==menuBefore.view40
            &&object->GetMenuStateForAnalysis().word48==menuBefore.word48&&object->GetMenuStateForAnalysis().borrowedInput54==menuBefore.borrowedInput54
            &&object->GetMenuStateForAnalysis().field58==menuBefore.field58,"Entry preserves other typed Menu fields");
        const auto after=host.Snapshot();
        if(emit)
        {
            std::cout<<"{\"stateAfterEntry\":"<<after<<",\"foreignEvents\":[";
            for(std::size_t i=0;i<host.events.size();++i)
            {if(i)std::cout<<',';const auto& e=host.events[i];std::cout<<"{\"part\":\""<<e.part<<"\",\"kind\":\""<<e.kind<<"\",\"receiverID\":"<<e.receiver<<",\"arguments\":";EntryHost::Numbers(std::cout,e.arguments);std::cout<<",\"before\":"<<e.before<<",\"after\":"<<e.after<<'}';}
            const auto& capture=host.captures.front();std::cout<<"],\"inputCaptures\":[{\"ownerID\":"<<capture.owner<<",\"resetHookID\":"<<capture.hook<<",\"rawMode44\":"<<capture.mode<<"}],\"parentTimerBytesUnchanged\":true}\n";
        }
        entryWatchedLevel=host.level.get();entryFreedLevel=0;Arm(*object);host.active=nullptr;object.reset();Closed(host);
        Check(!host.level&&entryFreedLevel==1,"Actual source owned GameLevel free, never fixture-cleared4C");entryWatchedLevel=nullptr;
    }
    void EntryContracts()
    {
        EntryOne({6,0,128,2,1,1},false);EntryOne({10,1,0,24,1,1},false);EntryOne({14,255,0,236,0x7FFFFFFF,1},false);EntryOne({15,0,0,0,0x80000000,0},false);
        EntryHost host(EntryConfig{});auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();object->MenuStateForFixture().object4C=host.level.get();
        const auto reject=[&]{Refuses([&]{object->FlowSlot20ForAnalysis();});Check(host.events.empty()&&host.captures.empty(),"Entry admission before any parent/own effect");};
        host.admitted=false;reject();wxMenuGameFlowState& menu=*object;Refuses([&]{(void)menu.MenuSlot44ForAnalysis();});host.admitted=true;
        object->MenuStateForFixture().object4C=nullptr;reject();object->MenuStateForFixture().object4C=host.level.get();
        host.level->SetName("");reject();host.level->SetName(nullptr);
        Check(host.level->AddInstanceForAnalysis(std::make_unique<spTemplateInstance>()),"Entry nonempty analytical guard fixture");reject();host.level->ClearInstancesForAnalysis();
        spBaseObject wrong;object->MenuStateForFixture().object4C=&wrong;reject();object->MenuStateForFixture().object4C=host.level.get();
        entryWatchedLevel=host.level.get();entryFreedLevel=0;Arm(*object);host.active=nullptr;object.reset();Closed(host);Check(entryFreedLevel==1,"Entry guard owned cleanup");entryWatchedLevel=nullptr;
    }
    void ReplayEntry()
    {
        EntryConfig c;while(std::cin>>c.index){Check(bool(std::cin>>c.parentInput18>>c.ownInput18>>c.mutations>>c.rawMode44>>c.cachedInputID),"Complete Entry replay row");EntryOne(c,true);}Check(std::cin.eof(),"Complete Entry replay input");
    }

    struct ExitConfig {unsigned index=0,ownedEmpty=0,targetPresent=1,mutations=0;std::uint32_t rawMode44=1;unsigned cachedInputID=1;};
    struct ExitHost final:Host
    {
        struct Event{std::string kind;unsigned receiver;std::vector<unsigned> arguments;std::string before,after;};
        ExitConfig exit;
        std::unique_ptr<spGameLevel> level;
        std::array<std::array<std::byte,8>,2> guis{},dxs{},devices{},owners{};
        std::array<std::array<std::byte,8>,4> hooks{};
        std::array<std::byte,8> target{},service{};
        std::array<unsigned,2> members{1,3};std::array<std::uint8_t,2> flags{0xA5,0x5A};
        unsigned gui=1,dx=1;bool admitted=true;std::vector<Event> events;
        explicit ExitHost(const ExitConfig& config):exit(config)
        {if(exit.ownedEmpty)level=std::make_unique<spGameLevel>();events.reserve(7);}
        std::string Snapshot() const
        {
            const auto& menu=active->GetMenuStateForAnalysis();std::ostringstream out;
            out<<"{\"view\":"<<menu.view40<<",\"mode\":"<<menu.mode44<<",\"field25\":"<<unsigned(active->GetStateForAnalysis().field25)
                <<",\"cachedInputID\":"<<InputID(active->GetGameOverStateForAnalysis().borrowedInput5C)<<",\"globalGUI\":"<<gui<<",\"globalDX\":"<<dx<<",\"globalOwner\":"<<owner<<",\"ownerMembers1C\":";
            EntryHost::Numbers(out,members);out<<",\"ownerFlags20\":";EntryHost::Numbers(out,flags);out<<",\"owned4CPresent\":"<<(menu.object4C?"true":"false")<<'}';return out.str();
        }
        Event Begin(const char* kind,unsigned receiver,std::vector<unsigned> arguments={}){return {kind,receiver,std::move(arguments),Snapshot(),{}};}
        void End(Event event){event.after=Snapshot();events.push_back(std::move(event));}
        ExitProfileForAnalysis GameOverExitProfileForAnalysis() const noexcept override
        {return admitted?ExitProfileForAnalysis::PCStableMenuBindings:ExitProfileForAnalysis::Unqualified;}
        std::unique_ptr<spBaseObject> TakeMenuObjectAllocationForAnalysis(spBaseObject& value) override
        {Check(&value==level.get(),"Exit actual owned level transfer");return std::move(level);}
        void* ResolveMenuGameContextForAnalysis() override{return this;}
        void* ReadMenuNotificationTarget2B4ForAnalysis(void* value) override{Check(value==this,"Exit contexttarget2B4");return exit.targetPresent?target.data():nullptr;}
        void DispatchNotificationForAnalysis(void* receiver,const spNotificationForAnalysis& packet) override
        {
            Check(receiver==target.data()&&packet.code==0x2764&&packet.word04==0&&packet.word08==0&&packet.word0C==0&&packet.sender==active&&packet.word14==0&&packet.payload==0&&packet.tail==0,"Exit inline logical eight-word directed frame");
            auto event=Begin("DirectedTarget",1,{0x2764,0,0,0,1,0,0,0});
            if(exit.mutations&1){active->MenuStateForFixture().view40=7;active->MutableStateForAnalysis().field25=0xEE;gui=dx=owner=2;}
            End(std::move(event));
        }
        void* ResolveMenuGUIManagerForAnalysis() override{return guis[gui-1].data();}
        void SetMenuViewEnabledForAnalysis(void* value,std::uint32_t view,std::uint32_t enabled) override
        {Check(view==active->GetMenuStateForAnalysis().view40&&enabled==0,"Exit capturedview/zero");auto event=Begin("GUIView",EntryHost::Identity(value,guis),{view,enabled});if(exit.mutations&2)active->MutableStateForAnalysis().field25=0xAA;End(std::move(event));}
        void* ResolveMenuAudioInterfaceForAnalysis() override{return dxs[dx-1].data();}
        void* GetMenuAudioControllerForAnalysis(void* value) override
        {const auto id=EntryHost::Identity(value,dxs);auto event=Begin("DXView18Getter0C",id);End(std::move(event));return devices[id-1].data();}
        void SetMenuAudioEnabledForAnalysis(void* value,std::uint32_t enabled) override
        {
            Check(enabled==0&&active->GetStateForAnalysis().field25==0,"Exit plain25clear precedes DXset0");auto event=Begin("DXDevice20",EntryHost::Identity(value,devices),{enabled});
            if(exit.mutations&4){gui=2;active->MutableStateForAnalysis().field25=0xEE;}End(std::move(event));
        }
        void SetMenuHUDForAnalysis(void* value,std::uint32_t enabled) override
        {Check(enabled==0,"Exit HUD0");auto event=Begin("GUIHUD",EntryHost::Identity(value,guis),{enabled});if(exit.mutations&8)active->MutableStateForAnalysis().field25=0xBB;End(std::move(event));}
        void* ResolveMenuExitServiceForAnalysis() override{return service.data();}
        void InvokeMenuExitServiceForAnalysis(void* value,std::uint32_t argument) override
        {Check(value==service.data()&&argument==1,"Exit foreign4628A0/1");auto event=Begin("ExitService4628A0",1,{argument});if(exit.mutations&16)owner=2;End(std::move(event));}
        void* ResolveMenuInputOwnerForAnalysis() override{return owners[owner-1].data();}
        void* ReadMenuInput1CForAnalysis(void* value) override{return hooks[members[EntryHost::Identity(value,owners)-1]-1].data();}
        void ResetMenuInputForAnalysis(void* value) override
        {
            const auto id=EntryHost::Identity(value,hooks),captured=(id-1)/2;Check(id==2*captured+1&&owner==captured+1,"Exit one freshowner/memberReset");auto event=Begin("UserInputReset20",id);
            if(exit.mutations&32){members[captured]=2*captured+2;owner=2-captured;active->MutableStateForAnalysis().field25=0xDD;active->MenuStateForFixture().mode44=0xDEADBEEF;}
            End(std::move(event));
        }
    };
    void ExitOne(const ExitConfig& config,bool emit)
    {
        Check(config.ownedEmpty<=1&&config.targetPresent<=1&&config.mutations<64&&config.cachedInputID<=1,"Bounded Exit input");ExitHost host(config);auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();
        object->MenuStateForFixture().object4C=host.level.get();object->MenuStateForFixture().mode44=config.rawMode44;object->MutableStateForAnalysis().field25=0xA5;object->GameOverStateForFixture().borrowedInput5C=config.cachedInputID?host.inputs[0].data():nullptr;
        const auto base=object->GetStateForAnalysis();const auto menu=object->GetMenuStateForAnalysis();const auto timers=TimerState(object->TimerForAnalysis()),extra=TimerState(*menu.timer50);const auto clocks=host.clockCalls;const auto borrowed=host.inputs;
        object->FlowSlot24ForAnalysis();Check(SameExcept25(base,object->GetStateForAnalysis())&&host.events.size()==6+config.targetPresent,"Whole inherited Exit effects and other Flow state");
        Check(TimerState(object->TimerForAnalysis())==timers&&TimerState(*menu.timer50)==extra&&host.clockCalls==clocks,"Exit preserves real parent timers/no clock");
        Check(object->GetMenuStateForAnalysis().object4C==host.level.get()&&object->GetMenuStateForAnalysis().allocation3C==menu.allocation3C&&object->GetMenuStateForAnalysis().word48==menu.word48&&object->GetMenuStateForAnalysis().borrowedInput54==menu.borrowedInput54&&object->GetMenuStateForAnalysis().field58==menu.field58,"Exit keeps ownlevel/other typed Menu fields");
        Check(host.inputs==borrowed&&object->GetGameOverStateForAnalysis().borrowedInput5C==(config.cachedInputID?host.inputs[0].data():nullptr),"Exit does not consume/change borrowed input5C");
        if(emit)
        {
            std::cout<<"{\"afterExit\":"<<host.Snapshot()<<",\"events\":[";
            for(std::size_t i=0;i<host.events.size();++i){if(i)std::cout<<',';const auto& e=host.events[i];std::cout<<"{\"kind\":\""<<e.kind<<"\",\"receiverID\":"<<e.receiver<<",\"arguments\":";EntryHost::Numbers(std::cout,e.arguments);std::cout<<",\"before\":"<<e.before<<",\"after\":"<<e.after<<'}';}
            std::cout<<"],\"parentTaskTimerBytesUnchanged\":true}\n";
        }
        entryWatchedLevel=host.level.get();entryFreedLevel=0;Arm(*object);host.active=nullptr;object.reset();Closed(host);Check(!host.level&&entryFreedLevel==config.ownedEmpty,"Exit actual optionallevel/state/timer lifetime");entryWatchedLevel=nullptr;
    }
    void ExitContracts()
    {
        ExitOne({0,0,1,0,1,1},false);ExitOne({9,1,1,63,0x80000000,1},false);ExitOne({2,0,0,0,0x80000000,0},false);
        ExitHost host(ExitConfig{1,1,1,0,1,1});auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();object->MenuStateForFixture().object4C=host.level.get();
        const auto reject=[&]{Refuses([&]{object->FlowSlot24ForAnalysis();});Check(host.events.empty(),"Exit guard precedes parent effects");};host.admitted=false;reject();host.admitted=true;host.level->SetName("");reject();host.level->SetName(nullptr);
        host.platform=Host::HookProfileForAnalysis::PS2;reject();host.platform=Host::HookProfileForAnalysis::PC;
        Check(host.level->AddInstanceForAnalysis(std::make_unique<spTemplateInstance>()),"Exit nonempty analytical guard fixture");reject();host.level->ClearInstancesForAnalysis();
        spBaseObject wrong;object->MenuStateForFixture().object4C=&wrong;reject();object->MenuStateForFixture().object4C=host.level.get();
        entryWatchedLevel=host.level.get();entryFreedLevel=0;Arm(*object);host.active=nullptr;object.reset();Closed(host);Check(entryFreedLevel==1,"Exit guard ownedcleanup");entryWatchedLevel=nullptr;
    }
    void ReplayExit(){ExitConfig c;while(std::cin>>c.index){Check(bool(std::cin>>c.ownedEmpty>>c.targetPresent>>c.mutations>>c.rawMode44>>c.cachedInputID),"Complete Exit row");ExitOne(c,true);}Check(std::cin.eof(),"Complete Exit input");}

    struct Slot28Config
    {
        unsigned index=0;std::uint32_t view=0x20,mode=1;
        unsigned cachedInputID=1,mutations=0,field25=0xA5,returnedWindowID=1;
    };
    struct Slot28Host final:Host
    {
        struct Event{std::string kind;unsigned receiver;std::vector<unsigned> arguments;std::string before,after;unsigned returnedWindowID=0;};
        Slot28Config slot;
        std::array<std::array<std::byte,8>,2> guis{},windows{};
        unsigned gui=1,guiResolutions=0,missingGUIOnResolution=0;
        bool admitted=true;std::vector<Event> events;
        explicit Slot28Host(const Slot28Config& config):slot(config){events.reserve(3);}
        Slot28ProfileForAnalysis GameOverSlot28ProfileForAnalysis() const noexcept override
        {return admitted?Slot28ProfileForAnalysis::PCStableGUIBindings:Slot28ProfileForAnalysis::Unqualified;}
        std::string Snapshot() const
        {
            const auto& menu=active->GetMenuStateForAnalysis();std::ostringstream out;
            out<<"{\"view\":"<<menu.view40<<",\"mode\":"<<menu.mode44<<",\"field25\":"<<unsigned(active->GetStateForAnalysis().field25)
               <<",\"cachedInputID\":"<<InputID(active->GetGameOverStateForAnalysis().borrowedInput5C)<<",\"globalGUI\":"<<gui<<'}';return out.str();
        }
        Event Begin(const char* kind,unsigned receiver,std::vector<unsigned> arguments)
        {return {kind,receiver,std::move(arguments),Snapshot(),{}};}
        void End(Event event){event.after=Snapshot();events.push_back(std::move(event));}
        void* ResolveMenuGUIManagerForAnalysis() override
        {return ++guiResolutions==missingGUIOnResolution?nullptr:guis[gui-1].data();}
        void* FindMenuWindowPCForAnalysis(void* value,std::uint32_t view) override
        {
            const auto id=EntryHost::Identity(value,guis);Check(id==1&&view==slot.view,"Slot28 captured GUI/view before lookup");
            auto event=Begin("GUILookup",id,{view});event.returnedWindowID=slot.returnedWindowID;
            if(slot.mutations&1)
            {
                active->MenuStateForFixture().view40=9;active->MenuStateForFixture().mode44=0xDEADBEEF;
                active->MutableStateForAnalysis().field25=0xEE;active->GameOverStateForFixture().borrowedInput5C=inputs[1].data();gui=2;
            }
            End(std::move(event));return windows[slot.returnedWindowID-1].data();
        }
        void ApplyMenuWindowPCForAnalysis(void* value,std::uint32_t flag) override
        {
            const auto id=EntryHost::Identity(value,windows);Check(id==slot.returnedWindowID&&flag==0,"Slot28 uses returned window with literal0");
            auto event=Begin("GUIApply",id,{flag});
            if(slot.mutations&2){active->MenuStateForFixture().view40=11;active->MutableStateForAnalysis().field25=0xBB;gui=3-gui;}
            End(std::move(event));
        }
        void SetMenuHUDForAnalysis(void* value,std::uint32_t flag) override
        {
            const auto id=EntryHost::Identity(value,guis);Check(id==gui&&flag==0,"Slot28 fresh GUI for HUD0");
            auto event=Begin("GUIHUD",id,{flag});if(slot.mutations&4)active->MutableStateForAnalysis().field25=0xDD;End(std::move(event));
        }
    };
    void Slot28One(const Slot28Config& config,bool emit)
    {
        Check(config.cachedInputID<=1&&config.mutations<8&&config.field25<=255&&config.returnedWindowID>=1&&config.returnedWindowID<=2,"Bounded slot28 replay row");
        Slot28Host host(config);auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();
        object->MenuStateForFixture().view40=config.view;object->MenuStateForFixture().mode44=config.mode;object->MutableStateForAnalysis().field25=static_cast<std::uint8_t>(config.field25);
        object->GameOverStateForFixture().borrowedInput5C=config.cachedInputID?host.inputs[0].data():nullptr;
        const auto base=object->GetStateForAnalysis();const auto menu=object->GetMenuStateForAnalysis();
        const auto timer=TimerState(object->TimerForAnalysis()),extra=TimerState(*menu.timer50);const auto clocks=host.clockCalls;const auto borrowed=host.inputs;
        wxMenuGameFlowState& menuView=*object;menuView.FlowSlot28ForAnalysis();
        Check(host.events.size()==3&&host.guiResolutions==2&&SameExcept25(base,object->GetStateForAnalysis()),"Whole inherited slot28 and fresh GUI observations");
        Check(TimerState(object->TimerForAnalysis())==timer&&TimerState(*menu.timer50)==extra&&host.clockCalls==clocks,"Slot28 preserves actual parent timers/no clock");
        Check(host.inputs==borrowed&&host.reads.empty()&&host.effects.empty(),"Slot28 does not consume borrowed inputs or flow");
        const auto& after=object->GetMenuStateForAnalysis();
        Check(after.object4C==nullptr&&after.allocation3C==nullptr&&after.timer50==menu.timer50&&after.word48==menu.word48&&after.borrowedInput54==menu.borrowedInput54&&after.field58==menu.field58,"Slot28 preserves other Menu state");
        const unsigned finalFlag=(config.mutations&4)?0xDD:(config.mutations&2)?0xBB:(config.mutations&1)?0xEE:config.field25;
        Check(object->GetStateForAnalysis().field25==finalFlag,"Slot28 has no trailing own25 clear");
        if(emit)
        {
            std::cout<<"{\"afterSlot28\":"<<host.Snapshot()<<",\"events\":[";
            for(std::size_t i=0;i<host.events.size();++i)
            {
                if(i)std::cout<<',';const auto& e=host.events[i];std::cout<<"{\"kind\":\""<<e.kind<<"\",\"receiverID\":"<<e.receiver<<",\"arguments\":";
                EntryHost::Numbers(std::cout,e.arguments);std::cout<<",\"before\":"<<e.before<<",\"after\":"<<e.after;
                if(e.returnedWindowID)std::cout<<",\"returnedWindowID\":"<<e.returnedWindowID;std::cout<<'}';
            }
            std::cout<<"],\"parentTaskTimerBytesUnchanged\":true,\"borrowedInputBytesUnchanged\":true}\n";
        }
        Arm(*object);host.active=nullptr;object.reset();Closed(host);
    }
    void Slot28Contracts()
    {
        Slot28One({0,0x20,1,1,0,0xA5,1},false);Slot28One({5,0x20,0x7FFFFFFF,0,7,0xA5,1},false);Slot28One({7,0x20,0,1,3,0x80,2},false);
        Slot28Host host(Slot28Config{});auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();object->MutableStateForAnalysis().field25=0x80;
        const auto before=host.Snapshot();const auto reject=[&]{Refuses([&]{object->FlowSlot28ForAnalysis();});Check(host.Snapshot()==before&&host.events.empty()&&host.guiResolutions==0,"Slot28 analytical admission before effects");};
        host.admitted=false;reject();host.admitted=true;host.platform=Host::HookProfileForAnalysis::PS2;reject();host.platform=Host::HookProfileForAnalysis::PC;
        spBaseObject foreign;object->MenuStateForFixture().object4C=&foreign;reject();object->MenuStateForFixture().object4C=nullptr;
        std::array<std::byte,8> buffer{};object->MenuStateForFixture().allocation3C=buffer.data();reject();object->MenuStateForFixture().allocation3C=nullptr;
        host.missingGUIOnResolution=2;Refuses([&]{object->FlowSlot28ForAnalysis();});Check(host.events.size()==2&&host.guiResolutions==2&&object->GetStateForAnalysis().field25==0x80,"Missing HUD binding preserves completed lookup/apply prefix; no rollback claim");
        Arm(*object);host.active=nullptr;object.reset();Closed(host);
    }
    void ReplaySlot28()
    {
        Slot28Config c;while(std::cin>>c.index)
        {Check(bool(std::cin>>c.view>>c.mode>>c.cachedInputID>>c.mutations>>c.field25>>c.returnedWindowID),"Complete slot28 replay row");Slot28One(c,true);}Check(std::cin.eof(),"Complete slot28 replay input");
    }

    struct Slot2CConfig
    {
        unsigned index=0;std::uint32_t mode=1;unsigned flag0=0,flag1=255,mutations=0,returnedWindowID=1;
        std::uint32_t view=0x20;unsigned cachedInputID=1;
    };
    struct Slot2CHost final:Host
    {
        struct Event{std::string kind;unsigned receiver;std::vector<unsigned> arguments;std::string before,after;unsigned returnedWindowID=0;};
        struct InputRead{unsigned inputID,offset,value;std::string before,after;};
        struct Capture{unsigned ownerID,resetHookID;std::uint32_t capturedRawMode;};
        Slot2CConfig slot;
        std::array<std::array<std::byte,8>,2> guis{},windows{},dxs{},devices{},owners{};
        std::array<std::array<std::byte,8>,4> hooks{};
        std::array<unsigned,2> members{1,3};std::array<std::uint8_t,2> sharedFlags{0x5A,0xA5};
        unsigned gui=1,dx=1,shared=1,memberReads=0,capturedFlag=0;
        bool admitted=true,missingDX=false;std::vector<Event> events;std::vector<InputRead> inputReads;std::vector<Capture> captures;
        explicit Slot2CHost(const Slot2CConfig& config):slot(config)
        {inputs[0][0x18]=static_cast<std::uint8_t>(slot.flag0);inputs[1][0x18]=static_cast<std::uint8_t>(slot.flag1);events.reserve(7);inputReads.reserve(1);captures.reserve(1);}
        Slot2CProfileForAnalysis GameOverSlot2CProfileForAnalysis() const noexcept override
        {return admitted?Slot2CProfileForAnalysis::PCStableInputGUIBindings:Slot2CProfileForAnalysis::Unqualified;}
        std::string Snapshot() const
        {
            const auto& menu=active->GetMenuStateForAnalysis();std::ostringstream out;
            out<<"{\"view\":"<<menu.view40<<",\"mode\":"<<menu.mode44<<",\"field25\":"<<unsigned(active->GetStateForAnalysis().field25)
               <<",\"cachedInputID\":"<<InputID(active->GetGameOverStateForAnalysis().borrowedInput5C)<<",\"globalGUI\":"<<gui<<",\"globalDX\":"<<dx
               <<",\"globalOwner\":"<<owner<<",\"globalShared\":"<<shared<<",\"ownerMembers1C\":";EntryHost::Numbers(out,members);
            out<<",\"inputFlags18\":["<<unsigned(inputs[0][0x18])<<','<<unsigned(inputs[1][0x18])<<"],\"shared45\":";EntryHost::Numbers(out,sharedFlags);out<<'}';return out.str();
        }
        Event Begin(const char* kind,unsigned receiver,std::vector<unsigned> arguments={}){return {kind,receiver,std::move(arguments),Snapshot(),{}};}
        void End(Event event){event.after=Snapshot();events.push_back(std::move(event));}
        void* ResolveMenuInputOwnerForAnalysis() override{return owners[owner-1].data();}
        void* ReadMenuInput1CForAnalysis(void* value) override
        {
            const auto id=EntryHost::Identity(value,owners);Check(id==1,"Slot2C SetMode reloads same captured owner");auto* captured=hooks[members[id-1]-1].data();
            if(++memberReads==1&&(slot.mutations&256))active->MenuStateForFixture().mode44=0xFFFFFFFF;
            return captured;
        }
        void ResetMenuInputForAnalysis(void* value) override
        {
            const auto id=EntryHost::Identity(value,hooks);Check(id==1&&memberReads==1,"Slot2C initial owner/member Reset once");
            captures.push_back({1,id,active->GetMenuStateForAnalysis().mode44});auto event=Begin("UserInputReset20",id);
            if(slot.mutations&1){members[0]=2;owner=2;active->MenuStateForFixture().mode44=0xDEADBEEF;active->MenuStateForFixture().view40=7;active->MutableStateForAnalysis().field25=0xA1;}
            End(std::move(event));
        }
        void SetMenuInputModeForAnalysis(void* value,std::uint32_t mode) override
        {
            const auto id=EntryHost::Identity(value,hooks);Check(memberReads==2&&id==((slot.mutations&1)?2u:1u)&&mode==captures[0].capturedRawMode,"Slot2C fresh same-owner member and captured raw mode");
            Check(mode<10||mode>=0x80000000,"Slot2C signed raw mode below10");auto event=Begin("UserInputSetMode1C",id,{mode});
            if(slot.mutations&128){active->MenuStateForFixture().view40=13;gui=2;active->MutableStateForAnalysis().field25=0xA7;}End(std::move(event));
        }
        void* ResolveMenuGUIManagerForAnalysis() override{return guis[gui-1].data();}
        void* FindMenuWindowPCForAnalysis(void* value,std::uint32_t view) override
        {
            const auto id=EntryHost::Identity(value,guis);Check(id==gui&&view==active->GetMenuStateForAnalysis().view40,"Slot2C fresh GUI/captured view");auto event=Begin("GUILookup",id,{view});event.returnedWindowID=slot.returnedWindowID;
            if(slot.mutations&2){owner=gui=2;active->MenuStateForFixture().view40=9;active->MutableStateForAnalysis().field25=0xB2;}
            End(std::move(event));return windows[slot.returnedWindowID-1].data();
        }
        void ApplyMenuWindowPCForAnalysis(void* value,std::uint32_t flag) override
        {
            const auto id=EntryHost::Identity(value,windows);Check(id==slot.returnedWindowID&&flag==1,"Slot2C captured returned window/literal1");auto event=Begin("GUIApply",id,{flag});
            if(slot.mutations&4){owner=3-owner;gui=3-gui;active->MutableStateForAnalysis().field25=0xC3;}End(std::move(event));
        }
        void* ReadMenuInput18ForAnalysis(void* value) override{return inputs[EntryHost::Identity(value,owners)-1].data();}
        std::uint8_t ReadMenuInputByte18ForAnalysis(void* value) override
        {
            const auto id=InputID(value);Check(id==owner,"Slot2C fresh owner/member18");capturedFlag=inputs[id-1][0x18];InputRead read{id,0x18,capturedFlag,Snapshot(),{}};
            if(slot.mutations&8){inputs[id-1][0x18]=capturedFlag?0:255;shared=2;}read.after=Snapshot();inputReads.push_back(std::move(read));return static_cast<std::uint8_t>(capturedFlag);
        }
        std::uint8_t& BorrowMenuSharedFlag45ForAnalysis() override{return sharedFlags[shared-1];}
        void* ResolveMenuAudioInterfaceForAnalysis() override{return missingDX?nullptr:dxs[dx-1].data();}
        void* GetMenuAudioControllerForAnalysis(void* value) override
        {
            const auto id=EntryHost::Identity(value,dxs);auto event=Begin("DXView18Getter0C",id);if(slot.mutations&16)dx=2;End(std::move(event));return devices[id-1].data();
        }
        void SetMenuAudioEnabledForAnalysis(void* value,std::uint32_t flag) override
        {
            Check(flag==unsigned(capturedFlag==0),"Slot2C captured byte18 selects device argument");auto event=Begin("DXDevice20",EntryHost::Identity(value,devices),{flag});
            if(slot.mutations&32){gui=2;active->MutableStateForAnalysis().field25=0xA5;}End(std::move(event));
        }
        void SetMenuHUDForAnalysis(void* value,std::uint32_t flag) override
        {
            const auto id=EntryHost::Identity(value,guis);Check(id==gui&&flag==unsigned(capturedFlag==0),"Slot2C fresh GUI/captured byte18 HUD argument");auto event=Begin("GUIHUD",id,{flag});
            if(slot.mutations&64){dx=2;active->MutableStateForAnalysis().field25=0xA6;}End(std::move(event));
        }
    };
    void Slot2COne(const Slot2CConfig& config,bool emit)
    {
        Check(config.cachedInputID<=1&&config.mutations<512&&config.flag0<=255&&config.flag1<=255&&config.returnedWindowID>=1&&config.returnedWindowID<=2,"Bounded slot2C replay row");
        Slot2CHost host(config);auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();object->MenuStateForFixture().view40=config.view;object->MenuStateForFixture().mode44=config.mode;
        object->MutableStateForAnalysis().field25=0xA5;object->GameOverStateForFixture().borrowedInput5C=config.cachedInputID?host.inputs[0].data():nullptr;
        const auto base=object->GetStateForAnalysis();const auto menu=object->GetMenuStateForAnalysis();const auto timer=TimerState(object->TimerForAnalysis()),extra=TimerState(*menu.timer50);const auto clocks=host.clockCalls;
        wxMenuGameFlowState& menuView=*object;menuView.FlowSlot2CForAnalysis();
        const auto mode=host.captures[0].capturedRawMode;const unsigned hasMode=mode<10||mode>=0x80000000;
        Check(host.events.size()==6+hasMode&&host.inputReads.size()==1&&host.captures.size()==1&&host.memberReads==1+hasMode,"Whole inherited slot2C event/input count");
        Check(SameExcept25(base,object->GetStateForAnalysis())&&object->GetStateForAnalysis().field25==0,"Slot2C final plain25 clear overwrites callback state");
        Check(TimerState(object->TimerForAnalysis())==timer&&TimerState(*menu.timer50)==extra&&host.clockCalls==clocks,"Slot2C preserves actual parent timers/no clock");
        Check(host.reads.empty()&&host.effects.empty()&&object->GetGameOverStateForAnalysis().borrowedInput5C==(config.cachedInputID?host.inputs[0].data():nullptr),"Slot2C does not read/change cached5C or invoke Tick flow");
        const auto& after=object->GetMenuStateForAnalysis();Check(after.object4C==nullptr&&after.allocation3C==nullptr&&after.timer50==menu.timer50&&after.word48==menu.word48&&after.borrowedInput54==menu.borrowedInput54&&after.field58==menu.field58,"Slot2C preserves other Menu state");
        Check(host.sharedFlags[host.shared-1]==unsigned(host.capturedFlag==0),"Slot2C writes fresh shared45 binding from captured flag");
        if(emit)
        {
            std::cout<<"{\"afterSlot2C\":"<<host.Snapshot()<<",\"events\":[";
            for(std::size_t i=0;i<host.events.size();++i){if(i)std::cout<<',';const auto& e=host.events[i];std::cout<<"{\"kind\":\""<<e.kind<<"\",\"receiverID\":"<<e.receiver<<",\"arguments\":";EntryHost::Numbers(std::cout,e.arguments);std::cout<<",\"before\":"<<e.before<<",\"after\":"<<e.after;if(e.returnedWindowID)std::cout<<",\"returnedWindowID\":"<<e.returnedWindowID;std::cout<<'}';}
            std::cout<<"],\"inputReads\":[";for(std::size_t i=0;i<host.inputReads.size();++i){if(i)std::cout<<',';const auto& r=host.inputReads[i];std::cout<<"{\"inputID\":"<<r.inputID<<",\"offset\":"<<r.offset<<",\"value\":"<<r.value<<",\"before\":"<<r.before<<",\"after\":"<<r.after<<'}';}
            std::cout<<"],\"inputCaptures\":[";for(std::size_t i=0;i<host.captures.size();++i){if(i)std::cout<<',';const auto& c=host.captures[i];std::cout<<"{\"ownerID\":"<<c.ownerID<<",\"resetHookID\":"<<c.resetHookID<<",\"capturedRawMode\":"<<c.capturedRawMode<<'}';}
            std::cout<<"],\"parentTaskTimerBytesUnchanged\":true}\n";
        }
        Arm(*object);host.active=nullptr;object.reset();Closed(host);
    }
    void Slot2CContracts()
    {
        Slot2COne({0,0,0,255,0,1,0x20,1},false);Slot2COne({13,10,255,0,1,1,0x20,1},false);Slot2COne({22,10,0,255,511,2,0xFFFFFFFF,0},false);
        Slot2CHost host(Slot2CConfig{12,9,0,255,1,1,0x20,1});auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();object->MenuStateForFixture().mode44=9;
        const auto before=host.Snapshot();const auto reject=[&]{Refuses([&]{object->FlowSlot2CForAnalysis();});Check(host.Snapshot()==before&&host.events.empty()&&host.inputReads.empty()&&host.memberReads==0,"Slot2C analytical admission before effects");};
        host.admitted=false;reject();host.admitted=true;host.platform=Host::HookProfileForAnalysis::PS2;reject();host.platform=Host::HookProfileForAnalysis::PC;
        spBaseObject foreign;object->MenuStateForFixture().object4C=&foreign;reject();object->MenuStateForFixture().object4C=nullptr;std::array<std::byte,8> buffer{};object->MenuStateForFixture().allocation3C=buffer.data();reject();object->MenuStateForFixture().allocation3C=nullptr;
        host.missingDX=true;Refuses([&]{object->FlowSlot2CForAnalysis();});Check(host.events.size()==5&&host.inputReads.size()==1&&host.sharedFlags[0]==0&&object->GetStateForAnalysis().field25==0xA1,"Missing DX binding preserves Reset/mode/window/shared/HUD prefix before finalclear; no rollback claim");
        Arm(*object);host.active=nullptr;object.reset();Closed(host);
    }
    void ReplaySlot2C()
    {
        Slot2CConfig c;while(std::cin>>c.index){Check(bool(std::cin>>c.mode>>c.flag0>>c.flag1>>c.mutations>>c.returnedWindowID>>c.view>>c.cachedInputID),"Complete slot2C replay row");Slot2COne(c,true);}Check(std::cin.eof(),"Complete slot2C replay input");
    }

    struct Slot38Config{unsigned index=0,field25=0;std::uint32_t view=0x20,mode=1;unsigned cachedInputID=1;};
    struct Slot38Host final:Host
    {
        bool admitted=true;
        Slot38ProfileForAnalysis GameOverSlot38ProfileForAnalysis() const noexcept override
        {return admitted?Slot38ProfileForAnalysis::PCStableOwnedLifetime:Slot38ProfileForAnalysis::Unqualified;}
        std::string Snapshot() const
        {
            const auto& menu=active->GetMenuStateForAnalysis();std::ostringstream out;
            out<<"{\"view\":"<<menu.view40<<",\"mode\":"<<menu.mode44<<",\"field25\":"<<unsigned(active->GetStateForAnalysis().field25)
               <<",\"cachedInputID\":"<<InputID(active->GetGameOverStateForAnalysis().borrowedInput5C)<<",\"parentBorrow30ID\":"<<InputID(active->GetBorrowedInputForAnalysis())
               <<",\"menuBorrow54ID\":"<<InputID(menu.borrowedInput54)<<",\"ownedBuffer3CPresent\":"<<(menu.allocation3C?"true":"false")<<",\"ownedLevel4CPresent\":"<<(menu.object4C?"true":"false")<<'}';return out.str();
        }
    };
    void Slot38One(const Slot38Config& config,bool emit)
    {
        Check(config.field25<=255&&config.cachedInputID<=2,"Bounded slot38 replay row");Slot38Host host;auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();
        object->MenuStateForFixture().view40=config.view;object->MenuStateForFixture().mode44=config.mode;object->MutableStateForAnalysis().field25=static_cast<std::uint8_t>(config.field25);
        object->GameOverStateForFixture().borrowedInput5C=config.cachedInputID?host.inputs[config.cachedInputID-1].data():nullptr;
        const auto before=host.Snapshot();const auto base=object->GetStateForAnalysis();const auto menu=object->GetMenuStateForAnalysis();const auto inputs=host.inputs;
        const auto timer=TimerState(object->TimerForAnalysis()),extra=TimerState(*menu.timer50);const auto clocks=host.clockCalls;
        wxMenuGameFlowState& menuView=*object;const bool returned=menuView.MenuSlot38ForAnalysis();
        Check(returned&&host.Snapshot()==before&&SameExcept25(base,object->GetStateForAnalysis())&&object->GetStateForAnalysis().field25==base.field25,"Slot38 wrapper/common-body true and unchanged typed receiver");
        Check(TimerState(object->TimerForAnalysis())==timer&&TimerState(*menu.timer50)==extra&&host.clockCalls==clocks&&host.inputs==inputs&&host.reads.empty()&&host.effects.empty(),"Slot38 preserves actual timers/borrows without foreign input or clock calls");
        const auto& after=object->GetMenuStateForAnalysis();Check(after.timer50==menu.timer50&&after.word48==menu.word48&&after.field58==menu.field58,"Slot38 preserves remaining typed Menu state");
        if(emit)std::cout<<"{\"beforeSlot38\":"<<before<<",\"afterSlot38\":"<<host.Snapshot()<<",\"returned\":true,\"parentTaskTimerBytesUnchanged\":true,\"borrowedInputBytesUnchanged\":true}\n";
        Arm(*object);host.active=nullptr;object.reset();Closed(host);
    }
    void Slot38Contracts()
    {
        Slot38One({0,0,0x20,1,1},false);Slot38One({2,0x80,0xFFFFFFFF,0xFFFFFFFF,2},false);Slot38One({3,0xFF,0x80000000,0x80000000,0},false);
        Slot38Host host;auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();
        const auto reject=[&]{const auto before=host.Snapshot();const auto clocks=host.clockCalls;Refuses([&]{(void)object->MenuSlot38ForAnalysis();});Check(host.Snapshot()==before&&host.reads.empty()&&host.effects.empty()&&host.clockCalls==clocks,"Slot38 analytical admission precedes effects");};
        host.admitted=false;reject();host.admitted=true;host.platform=Host::HookProfileForAnalysis::PS2;reject();host.platform=Host::HookProfileForAnalysis::PC;
        spBaseObject foreign;object->MenuStateForFixture().object4C=&foreign;reject();object->MenuStateForFixture().object4C=nullptr;std::array<std::byte,8> buffer{};object->MenuStateForFixture().allocation3C=buffer.data();reject();object->MenuStateForFixture().allocation3C=nullptr;
        const auto before=host.Snapshot();Refuses([&]{object->FlowSlot28ForAnalysis();});Refuses([&]{object->FlowSlot2CForAnalysis();});Check(host.Snapshot()==before&&host.reads.empty()&&host.effects.empty(),"Slot38 admission does not admit neighboring profiles");
        Arm(*object);host.active=nullptr;object.reset();Closed(host);
    }
    void ReplaySlot38()
    {Slot38Config c;while(std::cin>>c.index){Check(bool(std::cin>>c.field25>>c.view>>c.mode>>c.cachedInputID),"Complete slot38 replay row");Slot38One(c,true);}Check(std::cin.eof(),"Complete slot38 replay input");}

    std::string HexBytes(const void* data,std::size_t bytes)
    {
        static constexpr char digits[]="0123456789abcdef";const auto* raw=static_cast<const unsigned char*>(data);std::string out;out.reserve(bytes*2);
        for(std::size_t i=0;i<bytes;++i){out+=digits[raw[i]>>4];out+=digits[raw[i]&15];}return out;
    }
    struct Slot40Path{bool present=false;std::vector<char> bytes;};
    Slot40Path DecodeSlot40Path(const std::string& encoded)
    {
        if(encoded=="-")return {};Slot40Path path;path.present=true;
        if(encoded!="_")
        {
            Check(encoded.size()%2==0&&encoded.size()<=126,"Bounded supplied path bytes");
            const auto nibble=[](char c){Check((c>='0'&&c<='9')||(c>='a'&&c<='f'),"Lowercase path hex");return c<='9'?c-'0':c-'a'+10;};
            for(std::size_t i=0;i<encoded.size();i+=2)path.bytes.push_back(static_cast<char>((nibble(encoded[i])<<4)|nibble(encoded[i+1])));
        }
        path.bytes.push_back(0);return path;
    }
    struct Slot40Host final:Host
    {
        struct Buffer{void* address;std::size_t bytes;unsigned id,freeCount=0;bool live=true;};
        std::vector<Buffer> buffers;std::vector<std::string> events;
        bool admitted=true,destructing=false;int call=0;
        Slot40Host(){buffers.reserve(3);events.reserve(8);}
        ~Slot40Host() override{for(auto& buffer:buffers)if(buffer.live)std::free(buffer.address);}
        Slot40ProfileForAnalysis GameOverSlot40ProfileForAnalysis() const noexcept override
        {return admitted?Slot40ProfileForAnalysis::PCStableDisjointOwnedPath:Slot40ProfileForAnalysis::Unqualified;}
        Buffer& LiveBuffer(const void* address)
        {for(auto i=buffers.rbegin();i!=buffers.rend();++i)if(i->address==address&&i->live)return *i;throw std::runtime_error("Unknown live owned path buffer");}
        unsigned MemberID()
        {const auto* address=registered->GetMenuStateForAnalysis().allocation3C;return address?LiveBuffer(address).id:0;}
        void Event(const char* kind,unsigned id,std::size_t bytes,unsigned before,unsigned after)
        {
            std::ostringstream out;out<<"{\"call\":"<<(destructing?-1:call)<<",\"phase\":\""<<(destructing?"destructor":"operation")<<"\",\"kind\":\""<<kind
                <<"\",\"id\":"<<id<<",\"bytes\":"<<bytes<<",\"memberBefore\":"<<before<<",\"memberAfter\":"<<after<<'}';events.push_back(out.str());
        }
        std::string Events(std::size_t first=0) const
        {std::ostringstream out;out<<'[';for(std::size_t i=first;i<events.size();++i){if(i!=first)out<<',';out<<events[i];}out<<']';return out.str();}
        void* AllocateMenuPathBufferForAnalysis(std::size_t bytes) override
        {
            Check(!destructing&&active&&bytes>=1&&bytes<=64&&MemberID()==0,"Length-sized storage after old member clear");
            auto* address=std::malloc(bytes);if(!address)throw std::bad_alloc();std::memset(address,0xCC,bytes);
            const auto id=static_cast<unsigned>(buffers.size()+1);buffers.push_back({address,bytes,id});Event("Allocate",id,bytes,0,MemberID());return address;
        }
        void FreeMenuAllocationForAnalysis(void* address) override
        {
            Check(registered&&registered->GetMenuStateForAnalysis().allocation3C==address,"Free observes live Menu owned member before source clear");
            auto& buffer=LiveBuffer(address);const auto member=MemberID();Event("Free",buffer.id,buffer.bytes,member,MemberID());++buffer.freeCount;buffer.live=false;std::free(address);
        }
        std::string Snapshot()
        {
            const auto& menu=active->GetMenuStateForAnalysis();std::ostringstream out;out<<"{\"buffer\":";
            if(menu.allocation3C){const auto& buffer=LiveBuffer(menu.allocation3C);out<<"{\"id\":"<<buffer.id<<",\"bytes\":"<<buffer.bytes<<",\"data\":\""<<HexBytes(buffer.address,buffer.bytes)<<"\"}";}
            else out<<"null";
            out<<",\"view\":"<<menu.view40<<",\"mode\":"<<menu.mode44<<",\"field25\":"<<unsigned(active->GetStateForAnalysis().field25)
               <<",\"cachedInputID\":"<<InputID(active->GetGameOverStateForAnalysis().borrowedInput5C)<<",\"parentBorrow30ID\":"<<InputID(active->GetBorrowedInputForAnalysis())
               <<",\"menuBorrow54ID\":"<<InputID(menu.borrowedInput54)<<'}';return out.str();
        }
    };
    void Slot40One(unsigned index,const std::vector<std::string>& encoded,bool emit)
    {
        Check(index<6&&!encoded.empty()&&encoded.size()<=3,"Bounded slot40 sequence");std::vector<Slot40Path> paths;paths.reserve(encoded.size());
        for(const auto& value:encoded)paths.push_back(DecodeSlot40Path(value));const auto beforePaths=paths;
        Slot40Host host;auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();
        const auto beforeSequence=host.Snapshot(),beforeInputs=HexBytes(host.inputs.data(),sizeof(host.inputs));const auto base=object->GetStateForAnalysis();const auto menu=object->GetMenuStateForAnalysis();
        const auto timer=TimerState(object->TimerForAnalysis()),extra=TimerState(*menu.timer50);const auto clocks=host.clockCalls;std::ostringstream steps;steps<<'[';
        for(std::size_t i=0;i<paths.size();++i)
        {
            host.call=static_cast<int>(i);const auto before=host.Snapshot();const auto eventStart=host.events.size();wxMenuGameFlowState& menuView=*object;
            menuView.MenuSlot40ForAnalysis(paths[i].present?paths[i].bytes.data():nullptr);
            const auto& after=object->GetMenuStateForAnalysis();
            if(paths[i].present)
            {
                const auto bytes=std::strlen(paths[i].bytes.data())+1;const auto& buffer=host.LiveBuffer(after.allocation3C);
                Check(buffer.bytes==bytes&&std::memcmp(buffer.address,paths[i].bytes.data(),bytes)==0,"Exact owned copy through first NUL");
            }
            else Check(!after.allocation3C,"Null path clears owned member without allocation");
            Check(after.object4C==menu.object4C&&after.timer50==menu.timer50&&after.word48==menu.word48&&after.field58==menu.field58&&after.view40==menu.view40&&after.mode44==menu.mode44&&after.borrowedInput54==menu.borrowedInput54,"Slot40 preserves remaining Menu state");
            Check(SameExcept25(base,object->GetStateForAnalysis())&&base.field25==object->GetStateForAnalysis().field25&&TimerState(object->TimerForAnalysis())==timer&&TimerState(*menu.timer50)==extra&&host.clockCalls==clocks,"Slot40 preserves actual parent state/timers");
            Check(HexBytes(host.inputs.data(),sizeof(host.inputs))==beforeInputs&&host.reads.empty()&&host.effects.empty(),"Slot40 leaves borrowed inputs without foreign operations");
            for(std::size_t j=0;j<paths.size();++j)Check(paths[j].bytes==beforePaths[j].bytes,"Stable complete borrowed paths including ignored tail");
            if(i)steps<<',';steps<<"{\"ordinal\":"<<i<<",\"before\":"<<before<<",\"after\":"<<host.Snapshot()<<",\"storageEvents\":"<<host.Events(eventStart)
                <<",\"parentTaskTimerBytesUnchanged\":true,\"borrowedInputBytesUnchanged\":true,\"borrowedPathBytesUnchanged\":true}";
        }
        steps<<']';const auto afterSequence=host.Snapshot();Arm(*object);host.destructing=true;object.reset();host.active=nullptr;Closed(host);
        for(const auto& buffer:host.buffers)Check(!buffer.live&&buffer.freeCount==1,"Every owned path buffer freed exactly once");
        Check(HexBytes(host.inputs.data(),sizeof(host.inputs))==beforeInputs,"Borrowed input survives owned teardown");
        for(std::size_t i=0;i<paths.size();++i)Check(paths[i].bytes==beforePaths[i].bytes,"Borrowed path survives owned teardown");
        if(emit)std::cout<<"{\"beforeSequence\":"<<beforeSequence<<",\"steps\":"<<steps.str()<<",\"afterSequence\":"<<afterSequence<<",\"storageEvents\":"<<host.Events()
            <<",\"allSelectedOwnedAllocationsFreedExactlyOnce\":true,\"borrowedPathsAndInputsSurviveCleanup\":true}\n";
    }
    void Slot40Contracts()
    {
        Slot40One(0,{"-"},false);Slot40One(1,{"_"},false);Slot40One(2,{"616c706861","6c6f6e6765722f70617468"},false);
        Slot40One(3,{"616c706861","-"},false);Slot40One(4,{"d090d091","610069676e6f726564"},false);Slot40One(5,{"616c706861","ff80","_"},false);
        Slot40Host host;auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();
        const auto reject=[&]{const auto before=host.Snapshot();Refuses([&]{object->MenuSlot40ForAnalysis("unused");});Check(host.Snapshot()==before&&host.events.empty(),"Slot40 admission before storage effects");};
        host.admitted=false;reject();host.admitted=true;host.platform=Host::HookProfileForAnalysis::PS2;reject();host.platform=Host::HookProfileForAnalysis::PC;
        spBaseObject foreign;object->MenuStateForFixture().object4C=&foreign;reject();object->MenuStateForFixture().object4C=nullptr;
        Refuses([&]{object->FlowSlot20ForAnalysis();});Refuses([&]{object->FlowSlot24ForAnalysis();});Refuses([&]{object->FlowSlot28ForAnalysis();});Refuses([&]{object->FlowSlot2CForAnalysis();});Refuses([&]{(void)object->MenuSlot38ForAnalysis();});
        Refuses([&]{(void)object->FlowSlot30ForAnalysis();});Refuses([&]{(void)object->MenuSlot44ForAnalysis();});spCloneManager clones;Refuses([&]{(void)object->vfunc_10(clones);});
        Check(host.events.empty()&&host.reads.empty()&&host.effects.empty(),"Slot40 admission leaves other profiles and Clone unqualified");
        Arm(*object);host.destructing=true;object.reset();host.active=nullptr;Closed(host);
    }
    void ReplaySlot40()
    {
        unsigned index,count;while(std::cin>>index)
        {Check(bool(std::cin>>count)&&count>=1&&count<=3,"Complete bounded slot40 replay count");std::vector<std::string> paths(count);for(auto& path:paths)Check(bool(std::cin>>path),"Complete slot40 replay path");Slot40One(index,paths,true);}
        Check(std::cin.eof(),"Complete slot40 replay input");
    }

    void Contracts()
    {
        Lifecycle(false);
        Host host;auto object=std::make_unique<wxGameOverGameFlowState>(host);host.active=object.get();object->MutableStateForAnalysis().field25=0x80;
        Refuses([&]{(void)object->FlowSlot30ForAnalysis();});Check(host.reads.empty()&&host.effects.empty()&&object->GetStateForAnalysis().field25==0x80,"Default unqualified guard before effects");
        Refuses([&]{object->FlowSlot20ForAnalysis();});Refuses([&]{object->FlowSlot24ForAnalysis();});Refuses([&]{object->FlowSlot28ForAnalysis();});Refuses([&]{object->FlowSlot2CForAnalysis();});
        const auto menuBefore=object->GetMenuStateForAnalysis();const auto inputBefore=object->GetGameOverStateForAnalysis().borrowedInput5C;
        const auto baseBefore=object->GetStateForAnalysis();const auto timerBefore=TimerState(object->TimerForAnalysis()),menuTimerBefore=TimerState(*menuBefore.timer50);
        const auto clocksBefore=host.clockCalls;
        wxMenuGameFlowState& menuView=*object;
        Refuses([&]{(void)menuView.MenuSlot38ForAnalysis();});Refuses([&]{menuView.MenuSlot40ForAnalysis("unqualified-path");});Refuses([&]{(void)menuView.MenuSlot44ForAnalysis();});
        Check(object->GetMenuStateForAnalysis().allocation3C==menuBefore.allocation3C&&object->GetMenuStateForAnalysis().object4C==menuBefore.object4C
            &&object->GetMenuStateForAnalysis().timer50==menuBefore.timer50&&object->GetMenuStateForAnalysis().view40==menuBefore.view40
            &&object->GetMenuStateForAnalysis().mode44==menuBefore.mode44&&object->GetMenuStateForAnalysis().word48==menuBefore.word48
            &&object->GetMenuStateForAnalysis().borrowedInput54==menuBefore.borrowedInput54&&object->GetMenuStateForAnalysis().field58==menuBefore.field58
            &&SameExcept25(baseBefore,object->GetStateForAnalysis())&&object->GetStateForAnalysis().field25==baseBefore.field25
            &&TimerState(object->TimerForAnalysis())==timerBefore&&TimerState(*menuBefore.timer50)==menuTimerBefore
            &&object->GetGameOverStateForAnalysis().borrowedInput5C==inputBefore&&host.reads.empty()&&host.effects.empty()
            &&host.unqualifiedParentCalls==0&&host.clockCalls==clocksBefore,"Unproved extended hooks refuse through Menu reference before parent/own effects");
        spCloneManager clones;Refuses([&]{(void)object->vfunc_10(clones);});
        Check(object->GetStateForAnalysis().field25==0x80&&host.reads.empty(),"Unqualified hooks/Clone preserve state");
        host.qualified=true;auto* original=object->GameOverStateForFixture().borrowedInput5C;object->GameOverStateForFixture().borrowedInput5C=nullptr;
        Refuses([&]{(void)object->FlowSlot30ForAnalysis();});Check(host.reads.empty()&&object->GetStateForAnalysis().field25==0x80,"Missing input refuses before effects");
        object->GameOverStateForFixture().borrowedInput5C=original;
        host.inputs[0][0x1B]=1;host.missingFlow=true;
        Refuses([&]{(void)object->FlowSlot30ForAnalysis();});Check(object->GetStateForAnalysis().field25==0&&host.effects.empty(),"Missing flow preserves completed flag-clear prefix; no rollback claim");
        Arm(*object);host.active=nullptr;object.reset();Closed(host);
        Host ps2;ps2.platform=Host::HookProfileForAnalysis::PS2;Refuses([&]{wxGameOverGameFlowState unsupported(ps2);});
        Check(ps2.borrows.empty()&&ps2.registerCalls==0&&!ps2.menuTimer,"Unsupported constructor profile refuses before parent effects");
        One({0,0,0,0,0,0,0,0,0,0},false);One({3,1,1,0,0,0,0,0,0,0},false);
        One({10,1,0,1,0,0,3,1,0,0},false);One({12,1,1,0,0,0,0,0,1,0},false);
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string_view(argv[1])=="--lifecycle")Lifecycle(true);
        else if(argc==2&&std::string_view(argv[1])=="--slot28-replay")ReplaySlot28();
        else if(argc==2&&std::string_view(argv[1])=="--slot2c-replay")ReplaySlot2C();
        else if(argc==2&&std::string_view(argv[1])=="--slot38-replay")ReplaySlot38();
        else if(argc==2&&std::string_view(argv[1])=="--slot40-replay")ReplaySlot40();
        else if(argc==2&&std::string_view(argv[1])=="--exit-replay")ReplayExit();
        else if(argc==2&&std::string_view(argv[1])=="--entry-replay")ReplayEntry();
        else if(argc==2&&std::string_view(argv[1])=="--replay")
        {
            Config c;while(std::cin>>c.index)
            {Check(bool(std::cin>>c.flag>>c.a1B>>c.a2A>>c.b1B>>c.b2A>>c.mutation>>c.afterRead>>c.flowMutation>>c.resolveMutation),"Complete replay input row");One(c,true);}
            Check(std::cin.eof(),"Complete replay input row");
        }
        else{Contracts();EntryContracts();ExitContracts();Slot28Contracts();Slot2CContracts();Slot38Contracts();Slot40Contracts();std::cout<<"GameOver constructor, Entry, Exit/28/2C, wrapper38, owned path40 and own Tick contracts passed\n";}
        return 0;
    }
    catch(const std::exception& error){watching=false;std::cerr<<"GameOver "<<error.what()<<'\n';return 1;}
}
