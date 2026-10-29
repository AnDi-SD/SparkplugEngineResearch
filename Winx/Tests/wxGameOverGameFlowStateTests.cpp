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
        else if(argc==2&&std::string_view(argv[1])=="--exit-replay")ReplayExit();
        else if(argc==2&&std::string_view(argv[1])=="--entry-replay")ReplayEntry();
        else if(argc==2&&std::string_view(argv[1])=="--replay")
        {
            Config c;while(std::cin>>c.index)
            {Check(bool(std::cin>>c.flag>>c.a1B>>c.a2A>>c.b1B>>c.b2A>>c.mutation>>c.afterRead>>c.flowMutation>>c.resolveMutation),"Complete replay input row");One(c,true);}
            Check(std::cin.eof(),"Complete replay input row");
        }
        else{Contracts();EntryContracts();ExitContracts();std::cout<<"GameOver constructor, Entry, inherited Exit and own Tick contracts passed\n";}
        return 0;
    }
    catch(const std::exception& error){watching=false;std::cerr<<"GameOver "<<error.what()<<'\n';return 1;}
}
