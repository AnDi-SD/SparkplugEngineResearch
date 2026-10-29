#include "Code/wxPressStartGameFlowState.h"
#include "Analysis/PC/wxPressStartGameFlowStateAbi.h"
#include "Code/SparkBase/spNotification.h"
#include "Code/SparkBase/spSubscriptionManager.h"
#include "Code/Sparkplug/spGameLevel.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/SparkplugPC/spPCProjectionManager.h"
#include "Analysis/Host/spProjectionManagerHost.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
using namespace winx::reconstruction;
namespace
{
    void Check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
    void* Token(std::uintptr_t value) {return reinterpret_cast<void*>(value);}
    unsigned Number(const void* value) {return static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(value));}
    [[noreturn]] void Unbound() {throw std::logic_error("Foreign provider outside PressStart ctor/Tick subset");}
    struct Host : wxPressStartGameFlowStateHost
    {
        spSubscriptionManager& subscriptions;
        wxGameFlowResourcesForAnalysis resources;
        std::vector<std::unique_ptr<spTaskTimer>> timers;
        wxPressStartGameFlowState* active=nullptr;
        unsigned borrowedInput=1,owner=1,flow=1,borrowReads=0,parentClockReads=0;
        unsigned registrations=0,unregistrations=0,mutations=0,targetPresent=1;
        bool changeInputOnSecondParentClock=false;
        std::array<std::uint8_t,2> samples{0,0};
        std::vector<std::array<unsigned,2>> inputReads;
        std::vector<std::string> effects;
        explicit Host(spSubscriptionManager& manager):subscriptions(manager) {}
        void* BorrowInputForAnalysis() override {++borrowReads;return Token(borrowedInput);}
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override
        {
            ++parentClockReads;
            if(changeInputOnSecondParentClock && parentClockReads==2)borrowedInput=2;
            return {1200,1};
        }
        wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() override {return resources;}
        HookProfileForAnalysis MenuHookProfileForAnalysis() const noexcept override {return HookProfileForAnalysis::PC;}
        void RegisterMenuReceiverForAnalysis(wxMenuGameFlowState& value,std::uint32_t tag) override
        {Check(tag==0x20 && subscriptions.SubscribeForAnalysis(tag,value),"Real source Menu subscription20");++registrations;}
        void UnregisterMenuReceiverForAnalysis(wxMenuGameFlowState& value,std::uint32_t tag) override
        {Check(tag==0x20 && subscriptions.UnsubscribeForAnalysis(tag,value),"Exact source Menu unsubscription20");++unregistrations;}
        void StoreMenuTimerAllocationForAnalysis(std::unique_ptr<spTaskTimer> value) override {timers.push_back(std::move(value));}
        std::unique_ptr<spTaskTimer> TakeMenuTimerAllocationForAnalysis(spTaskTimer& value) override
        {
            for(auto& timer:timers)if(timer.get()==&value)return std::move(timer);
            throw std::logic_error("Timer storage must return the exact owned pointer");
        }
        std::unique_ptr<spBaseObject> TakeMenuObjectAllocationForAnalysis(spBaseObject&) override {Unbound();}
        void FreeMenuAllocationForAnalysis(void*) override {Unbound();}
        std::uint8_t ReadPressStartCachedInputByte1BForAnalysis(void* input) override
        {
            Check(Number(input)==1 || Number(input)==2,"Actual own cached input binding");
            const auto sample=samples[std::min(inputReads.size(),std::size_t(1))];
            inputReads.push_back({Number(input),sample});return sample;
        }
        void* ResolveMenuGameContextForAnalysis() override {return Token(100);}
        void* ReadMenuNotificationTarget2B4ForAnalysis(void* context) override
        {Check(context==Token(100),"Current context receiver");return targetPresent?Token(200):nullptr;}
        void DispatchNotificationForAnalysis(void* target,const spNotificationForAnalysis& packet) override
        {
            Check(active && target==Token(200) && packet.sender==active,"Directed target and source receiver");
            Check(packet.code==0x27D1 && packet.word04==0 && packet.word08==0 && packet.word0C==0
                && packet.word14==0 && packet.payload==0x11 && packet.tail==0,"All eight packet words");
            auto& state=active->PressStartStateForFixture();
            effects.push_back("N1:10193:0:0:0:1:0:17:0:"+std::to_string(state.field60));
            if(mutations&1)state.borrowedInput5C=Token(2);
            if(mutations&2)flow=2;
            if(mutations&4)state.field60=0xEE;
        }
        void* ResolveMenuFlowManagerForAnalysis() override {return Token(300+flow);}
        void InvokeMenuFlow595450ForAnalysis(void* receiver,std::uint32_t code,std::uint32_t value) override
        {
            Check(active && (receiver==Token(301) || receiver==Token(302)),"Fresh foreign flow receiver");
            Check(code==0x38 && value==0,"Exact foreign flow command");
            auto& state=active->PressStartStateForFixture();
            effects.push_back("F"+std::to_string(Number(receiver)-300)+":56:0:"+std::to_string(state.field60));
            if(mutations&8) {state.field60=0xA5;state.borrowedInput5C=Token(2);owner=2;}
        }
        // Legacy Menu Audio-named interface is an unqualified foreign
        // spDXInputManager/view/device binding, never an audio implementation.
        void* ResolveMenuAudioInterfaceForAnalysis() override {Unbound();}
        void* GetMenuAudioControllerForAnalysis(void*) override {Unbound();}
        void SetMenuAudioEnabledForAnalysis(void*,std::uint32_t) override {Unbound();}
        void* ResolveMenuGUIManagerForAnalysis() override {Unbound();}
        void SetMenuViewEnabledForAnalysis(void*,std::uint32_t,std::uint32_t) override {Unbound();}
        void* FindMenuWindowPCForAnalysis(void*,std::uint32_t) override {Unbound();}
        void ApplyMenuWindowPCForAnalysis(void*,std::uint32_t) override {Unbound();}
        void* FindMenuWindowPS2ForAnalysis(void*,std::uint32_t) override {Unbound();}
        void ApplyMenuWindowPS2ForAnalysis(void*,std::uint32_t) override {Unbound();}
        void SetMenuHUDForAnalysis(void*,std::uint32_t) override {Unbound();}
        void* ResolveMenuExitServiceForAnalysis() override {Unbound();}
        void InvokeMenuExitServiceForAnalysis(void*,std::uint32_t) override {Unbound();}
        void* ResolveMenuInputOwnerForAnalysis() override {Unbound();}
        void* ReadMenuInput1CForAnalysis(void*) override {Unbound();}
        void* ReadMenuInput18ForAnalysis(void*) override {Unbound();}
        std::uint8_t ReadMenuInputByte18ForAnalysis(void*) override {Unbound();}
        void ResetMenuInputForAnalysis(void*) override {Unbound();}
        void SetMenuInputModeForAnalysis(void*,std::uint32_t) override {Unbound();}
        std::uint8_t& BorrowMenuSharedFlag45ForAnalysis() override {Unbound();}
        bool AllTimersReleased() const
        {return std::all_of(timers.begin(),timers.end(),[](const auto& timer){return !timer;});}
    };
    void AppendTimer(std::ostream& output,const spTaskTimer& timer)
    {
        const auto& s=timer.GetStateForAnalysis();std::uint32_t bits;
        std::memcpy(&bits,&s.deltaSeconds,sizeof(bits));
        output<<s.active<<','<<s.relative<<','<<s.currentMilliseconds<<','<<s.startMilliseconds<<','<<s.pausedMilliseconds<<','<<bits<<';';
    }
    std::string GuardedState(const wxPressStartGameFlowState& object)
    {
        const auto& b=object.GetStateForAnalysis();const auto& m=object.GetMenuStateForAnalysis();
        const auto& t=object.EmbeddedTimerForAnalysis().GetStateForAnalysis();std::ostringstream out;
        out<<b.field10<<','<<unsigned(b.field14)<<',';
        for(const auto code:b.codes)out<<code<<',';
        out<<unsigned(b.field24)<<','<<unsigned(b.field25)<<','<<b.field34<<','<<unsigned(b.field38)<<','
            <<object.GetBorrowedInputForAnalysis()<<','<<m.allocation3C<<','<<m.view40<<','<<m.mode44<<','<<m.word48<<','
            <<m.object4C<<','<<m.timer50<<','<<m.borrowedInput54<<','<<unsigned(m.field58)<<';';
        AppendTimer(out,object.TimerForAnalysis());Check(m.timer50!=nullptr,"Live parent Menu timer");AppendTimer(out,*m.timer50);
        out<<unsigned(t.active)<<','<<t.accumulated<<','<<t.startedAt<<','<<unsigned(t.limited)<<','<<t.limit;
        return out.str();
    }
    void Print(const wxPressStartGameFlowState& object,const Host& host,bool returned)
    {
        const auto& s=object.GetPressStartStateForAnalysis();const auto& t=object.EmbeddedTimerForAnalysis().GetStateForAnalysis();
        Check(s.capturedWord64.has_value(),"Replay explicitly supplies raw64 after construction");
        std::cout<<"{\"returned\":"<<returned<<",\"state\":["<<unsigned(s.field60)<<','<<Number(s.borrowedInput5C)<<','
            <<*s.capturedWord64<<','<<host.flow<<','<<host.owner<<','<<unsigned(t.active)<<','<<t.accumulated<<','<<t.startedAt<<','
            <<unsigned(t.limited)<<','<<t.limit<<','<<host.inputReads.size()<<",0],\"effects\":[";
        for(std::size_t i=0;i<host.effects.size();++i) {if(i)std::cout<<',';std::cout<<'"'<<host.effects[i]<<'"';}
        // Final scalar is structurally zero: this subset constructs Timer(0)
        // with an empty clock source and never calls a Timer clock operation.
        std::cout<<"],\"inputReads\":[";
        for(std::size_t i=0;i<host.inputReads.size();++i)
        {if(i)std::cout<<',';std::cout<<"{\"inputID\":"<<host.inputReads[i][0]<<",\"value\":"<<host.inputReads[i][1]<<'}';}
        std::cout<<"],\"parentStateAndEmbeddedTimerUnchanged\":true}\n";
    }
    void Replay()
    {
        std::string line;
        while(std::getline(std::cin,line))
        {
            if(line.empty())continue;
            unsigned flag,a,b,mutations,target;std::uint32_t raw64;std::istringstream input(line);
            Check(bool(input>>flag>>a>>b>>mutations>>target>>raw64) && flag<256 && a<256 && b<256 && target<2,"Six PressStart fixture tokens");
            spSubscriptionManager subscriptions;Host host(subscriptions);
            {
                wxPressStartGameFlowState object(host);host.active=&object;
                host.samples={static_cast<std::uint8_t>(a),static_cast<std::uint8_t>(b)};host.mutations=mutations;host.targetPresent=target;
                auto& state=object.PressStartStateForFixture();state.field60=static_cast<std::uint8_t>(flag);state.capturedWord64=raw64;
                const auto before=GuardedState(object);const bool returned=object.FlowSlot30ForAnalysis();
                Check(before==GuardedState(object) && state.capturedWord64==raw64,"Tick preserves parent/timer/captured64 state");Print(object,host,returned);
            }
            Check(host.AllTimersReleased() && subscriptions.GetSubscriberCountForAnalysis(0x20)==0,"Replay cleanup owns real timers/subscriptions");
        }
    }
    void Contracts()
    {
        bool refused=false;
        try {(void)spRTTIManager::Instance().Create(wxPressStartGameFlowState::ClassID);}catch(const std::logic_error&) {refused=true;}
        Check(refused,"Missing factory host refuses construction");
        spSubscriptionManager subscriptions;Host host(subscriptions);
        wxPressStartGameFlowState::SetFactoryHostForAnalysis(&host);
        {
            auto sourceBase=spRTTIManager::Instance().Create(wxPressStartGameFlowState::ClassID);
            auto* source=dynamic_cast<wxPressStartGameFlowState*>(sourceBase.get());Check(source!=nullptr,"RTTI creates actual PressStart");host.active=source;
            Check(source->vfunc_18().IsExactly(wxPressStartGameFlowState::ClassID) && source->vfunc_18().IsKindOf(wxMenuGameFlowState::ClassID),"Typed RTTI chain");
            Check(host.borrowReads==3 && host.parentClockReads==2,"Three independent input reads and two parent TaskTimer clocks");
            const auto& menu=source->GetMenuStateForAnalysis();const auto& timer=source->EmbeddedTimerForAnalysis().GetStateForAnalysis();
            Check(menu.view40==8 && menu.mode44==0 && source->GetPressStartStateForAnalysis().borrowedInput5C==Token(1),"Own constructor defaults");
            Check(!source->GetPressStartStateForAnalysis().capturedWord64 && timer.active==0 && timer.accumulated==0 && timer.startedAt==0 && timer.limited==0 && timer.limit==0,"Unknown64 and actual shared inactive Timer defaults");
            wxPressStartGameFlowState destination(host);auto& target=destination.PressStartStateForFixture();target.field60=17;target.capturedWord64=0x87654321;
            auto& state=source->PressStartStateForFixture();state.field60=255;state.capturedWord64=0x12345678;
            spCloneManager clones;Check(source->vfunc_14(destination,clones) && target.field60==17 && target.capturedWord64==0x87654321,"Copy literal no-op");
            host.borrowedInput=2;auto cloneBase=clones.Clone(*source);auto* clone=dynamic_cast<wxPressStartGameFlowState*>(cloneBase.get());
            Check(clone && clone->GetBorrowedInputForAnalysis()==Token(2) && clone->GetPressStartStateForAnalysis().borrowedInput5C==Token(2)
                && clone->GetPressStartStateForAnalysis().field60==0 && !clone->GetPressStartStateForAnalysis().capturedWord64,"Clone freshly constructs input/flag/unknown state");
            Check(subscriptions.GetSubscriberCountForAnalysis(0x20)==3 && host.registrations==3,"Three real shared Menu subscriptions");
            const auto before=GuardedState(*source);const auto reads=host.parentClockReads;
            refused=false;try {source->FlowSlot20ForAnalysis();}catch(const std::logic_error&) {refused=true;}
            Check(refused && before==GuardedState(*source),"Entry refuses before effects");
            refused=false;try {source->FlowSlot24ForAnalysis();}catch(const std::logic_error&) {refused=true;}
            Check(refused && before==GuardedState(*source) && host.effects.empty() && host.inputReads.empty() && reads==host.parentClockReads,"Exit refuses before effects");
            state.borrowedInput5C=nullptr;refused=false;
            try {(void)source->FlowSlot30ForAnalysis();}catch(const std::logic_error&) {refused=true;}
            Check(refused && state.field60==255 && host.effects.empty() && host.inputReads.empty(),"Absent input binding refuses before Tick effects");
        }
        wxPressStartGameFlowState::SetFactoryHostForAnalysis(nullptr);
        Check(host.AllTimersReleased() && host.registrations==3 && host.unregistrations==3 && subscriptions.GetSubscriberCountForAnalysis(0x20)==0,"All parent timers/subscriptions retired");
        Host reload(subscriptions);reload.changeInputOnSecondParentClock=true;
        {
            wxPressStartGameFlowState object(reload);
            Check(object.GetBorrowedInputForAnalysis()==Token(1) && object.GetMenuStateForAnalysis().borrowedInput54==Token(1)
                && object.GetPressStartStateForAnalysis().borrowedInput5C==Token(2),"Own input read is fresh after parent clock mutation");
        }
        Check(reload.AllTimersReleased(),"Fresh-input contract cleanup");
    }
    struct EntryHost : Host,spProjectionManagerHost
    {
        std::unique_ptr<spGameLevel> level=std::make_unique<spGameLevel>();
        spPCProjectionManager projection;
        spNode root;
        std::array<unsigned,2> ownerMembers{1,3};
        std::array<std::uint8_t,2> ownerFlags{0xA5,0x5A},sharedFlags{0x5A,0xA5};
        unsigned gui=1,shared=1,dx=1,inputByte=0;
        bool admitted=true,ownTail=false;
        struct Capture {unsigned owner;std::uint32_t mode;unsigned receiver;};
        struct ClockObservation {std::uint32_t sample,raw64;unsigned owner;std::array<std::uint8_t,2> flags;std::array<std::uint32_t,5> timer;};
        std::vector<Capture> captures;
        std::vector<ClockObservation> clockObservations;
        explicit EntryHost(spSubscriptionManager& manager):Host(manager)
        {
            root.SetPositionForAnalysis({1,2,3});
            // Exact captured input: this native fixture has not executed the
            // matrix startup6D38E0. Its local orientation40..63 is nine zeros.
            root.SetOrientationForAnalysis(spNode::Matrix3{});
            root.MarkLocalTransformDirtyForAnalysis();
        }
        static std::array<std::uint32_t,5> TimerState(const spTimer& timer)
        {const auto& s=timer.GetStateForAnalysis();return {s.active,s.accumulated,s.startedAt,s.limited,s.limit};}
        EntryProfileForAnalysis PressStartEntryProfileForAnalysis() const noexcept override
        {return admitted?EntryProfileForAnalysis::PCEmptyOwnedGameLevel:EntryProfileForAnalysis::Unqualified;}
        std::unique_ptr<spBaseObject> TakeMenuObjectAllocationForAnalysis(spBaseObject& value) override
        {Check(&value==level.get(),"Exact source owned empty GameLevel Take");return std::move(level);}
        void* ResolveMenuEngineCoreForAnalysis() override{return Token(701);}
        void* ReadMenuScene18ForAnalysis(void* value) override{Check(value==Token(701),"Core18");return Token(702);}
        spProjectionManager* ReadMenuProjectionManager2CForAnalysis(void* value) override
        {Check(value==Token(702),"Scene2C");return &projection;}
        spNode* ReadMenuSystemRoot14ForAnalysis(void* value) override
        {Check(value==Token(702),"Scene14");return &root;}
        spProjectionManagerHost& MenuProjectionHostForAnalysis() override{return *this;}
        void* ReadProjectionHelper94ForAnalysis(void*) override{Unbound();}
        void WriteProjectionHelperOwner18ForAnalysis(void*,void*) override{Unbound();}
        std::uint32_t InvokeProjectionHelper0ForAnalysis(void*) override{Unbound();}
        std::uint32_t DrawProjection38ForAnalysis(void*,void*) override{Unbound();}
        void* ResolveMenuGUIManagerForAnalysis() override{return Token(400+gui);}
        void SetMenuViewEnabledForAnalysis(void* value,std::uint32_t view,std::uint32_t enabled) override
        {effects.push_back("V"+std::to_string(Number(value)-400)+':'+std::to_string(view)+':'+std::to_string(enabled));}
        void SetMenuHUDForAnalysis(void* value,std::uint32_t enabled) override
        {
            effects.push_back("H"+std::to_string(Number(value)-400)+':'+std::to_string(enabled));
            if(ownTail&&(mutations&32)){owner=2;active->MenuStateForFixture().mode44=10;}
        }
        void* FindMenuWindowPCForAnalysis(void* value,std::uint32_t view) override
        {effects.push_back("L"+std::to_string(Number(value)-400)+':'+std::to_string(view));return Token(703);}
        void InvokePressStartWindowConsumer55D570ForAnalysis(void* window,std::uint32_t literal) override
        {
            Check(window==Token(703)&&literal==0x707F94,"Opaque window consumer/literal binding");ownTail=true;effects.push_back("W:literal707F94");
            if(mutations&8){gui=shared=dx=owner=2;active->MenuStateForFixture().view40=9;}
            if(mutations&16)active->MenuStateForFixture().mode44=0xFFFFFFFF;
        }
        void* ResolveMenuInputOwnerForAnalysis() override{return Token(100+owner);}
        void* ReadMenuInput18ForAnalysis(void* value) override
        {const auto id=Number(value)-100;Check(id==1||id==2,"Input owner18");return Token(id);}
        std::uint8_t ReadMenuInputByte18ForAnalysis(void* value) override
        {const auto id=Number(value);Check(id==1||id==2,"Input byte18");return static_cast<std::uint8_t>(id==1?inputByte:255-inputByte);}
        std::uint8_t& BorrowMenuSharedFlag45ForAnalysis() override{return sharedFlags[shared-1];}
        void* ResolvePressStartDXInputManagerForAnalysis() override{return Token(500+dx);}
        void* GetDevice(void* value)
        {const auto id=Number(value)-500;Check(id==1||id==2,"DXInputManager view18 binding");effects.push_back("DX"+std::to_string(id)+":Get");return Token(600+id);}
        void SetDevice(void* value,std::uint32_t enabled,bool own)
        {
            const auto id=Number(value)-600;Check(id==1||id==2,"Foreign input device20 binding");effects.push_back("DX"+std::to_string(id)+":Set:"+std::to_string(enabled));
            if(own&&(mutations&64)){owner=2;active->MenuStateForFixture().mode44=0;}
        }
        void* InvokePressStartDXInputManagerView18Slot0CForAnalysis(void* value) override{return GetDevice(value);}
        void SetPressStartDXInputDeviceSlot20ForAnalysis(void* value,std::uint32_t enabled) override{SetDevice(value,enabled,true);}
        // The unchanged parent legacy names expose the same confirmed foreign
        // DXInputManager/view/device protocol; no audio body is supplied.
        void* ResolveMenuAudioInterfaceForAnalysis() override{return ResolvePressStartDXInputManagerForAnalysis();}
        void* GetMenuAudioControllerForAnalysis(void* value) override{return GetDevice(value);}
        void SetMenuAudioEnabledForAnalysis(void* value,std::uint32_t enabled) override{SetDevice(value,enabled,false);}
        void* ReadMenuInput1CForAnalysis(void* value) override
        {const auto id=Number(value)-100;Check(id==1||id==2,"Captured input owner1C");return Token(10+ownerMembers[id-1]);}
        void ResetMenuInputForAnalysis(void* value) override
        {
            const auto receiver=Number(value)-10;Check(receiver>=1&&receiver<=4,"Live input Reset receiver");const auto capturedOwner=(receiver-1)/2;
            auto& menu=active->MenuStateForFixture();auto& state=active->PressStartStateForFixture();Check(state.capturedWord64.has_value(),"Explicit raw64 fixture capture");
            captures.push_back({capturedOwner+1,menu.mode44,receiver});effects.push_back("R"+std::to_string(receiver)+':'+std::to_string(menu.mode44)+':'+std::to_string(*state.capturedWord64));
            if(mutations&1){ownerMembers[capturedOwner]=2*capturedOwner+2;menu.mode44=menu.mode44==10?0:10;state.capturedWord64=0xDEADBEEF;}
            if(mutations&2)owner=2-capturedOwner;
        }
        void SetMenuInputModeForAnalysis(void* value,std::uint32_t mode) override
        {
            Check(!captures.empty()&&Number(value)==10+ownerMembers[captures.back().owner-1]&&mode==captures.back().mode,"Captured mode and reloaded captured-owner member");
            effects.push_back("M"+std::to_string(Number(value)-10)+':'+std::to_string(mode));
            if(mutations&4){owner=2;ownerFlags[1]=0xEE;active->PressStartStateForFixture().capturedWord64=0xFEEDBEEF;}
        }
        std::uint8_t& BorrowPressStartInputOwnerFlag20ForAnalysis(void* value) override
        {const auto id=Number(value)-100;Check(id==1||id==2,"Owner flag20 live structural borrow");return ownerFlags[id-1];}
        std::uint32_t ReadPressStartTimerClockForAnalysis() override
        {
            Check(clockObservations.size()<2,"Distinct Reset/Start clock observations");const auto sample=1000+17*static_cast<std::uint32_t>(clockObservations.size());
            auto& state=active->PressStartStateForFixture();Check(state.capturedWord64.has_value(),"Known word64 before clock");
            clockObservations.push_back({sample,*state.capturedWord64,owner,ownerFlags,TimerState(active->EmbeddedTimerForAnalysis())});
            effects.push_back("C:"+std::to_string(sample)+':'+std::to_string(*state.capturedWord64)+':'+std::to_string(ownerFlags[0])+':'+std::to_string(ownerFlags[1])+':'+std::to_string(owner));
            if(clockObservations.size()==1)
            {
                Check(*state.capturedWord64==0&&ownerFlags[owner-1]==1,"Own flag/word stores precede first clock");
                if(mutations&128){state.capturedWord64=0xAABBCCDD;owner=2;ownerFlags[1]=0xEE;}
                if(mutations&256)
                {auto timer=active->EmbeddedTimerForAnalysis().GetStateForAnalysis();timer.limited=255;timer.limit=0xFFFFFFFF;active->EmbeddedTimerForFixture().SetStateForAnalysis(timer);}
            }
            return sample;
        }
    };
    template<class Values>void PrintNumbers(const Values& values)
    {std::cout<<'[';bool comma=false;for(const auto value:values){if(comma)std::cout<<',';comma=true;std::cout<<static_cast<std::uint32_t>(value);}std::cout<<']';}
    std::vector<std::uint32_t> EntryNodeWords(const spNode& node)
    {
        std::vector<std::uint32_t> result;
        const auto append=[&](const auto& values){for(float value:values){std::uint32_t bits;std::memcpy(&bits,&value,4);result.push_back(bits);}};
        append(node.GetWorldPositionForAnalysis());append(node.GetWorldScaleForAnalysis());append(node.GetWorldOrientationForAnalysis());result.push_back(node.GetFlagsForAnalysis());return result;
    }
    void PrintEntry(const wxPressStartGameFlowState& object,const EntryHost& host)
    {
        const auto& state=object.GetPressStartStateForAnalysis();const auto& menu=object.GetMenuStateForAnalysis();
        std::cout<<"{\"stateAfterEntry\":{\"field60\":"<<unsigned(state.field60)<<",\"cachedInputID\":"<<Number(state.borrowedInput5C)<<",\"raw64\":"<<*state.capturedWord64
            <<",\"view\":"<<menu.view40<<",\"mode\":"<<menu.mode44<<",\"field25\":"<<unsigned(object.GetStateForAnalysis().field25)<<",\"shared45\":";
        PrintNumbers(host.sharedFlags);std::cout<<",\"globalGUI\":"<<host.gui<<",\"globalShared\":"<<host.shared<<",\"globalDX\":"<<host.dx<<",\"globalOwner\":"<<host.owner<<",\"ownerMembers1C\":";
        PrintNumbers(host.ownerMembers);std::cout<<",\"ownerFlags20\":";PrintNumbers(host.ownerFlags);std::cout<<",\"embeddedTimer\":";PrintNumbers(EntryHost::TimerState(object.EmbeddedTimerForAnalysis()));
        std::cout<<",\"rootWords\":";PrintNumbers(EntryNodeWords(host.root));std::cout<<",\"projectionEnabled\":"<<unsigned(host.projection.GetEnabledByteForAnalysis())<<",\"WINMMsamples\":";
        std::vector<std::uint32_t> samples;for(const auto& clock:host.clockObservations)samples.push_back(clock.sample);PrintNumbers(samples);std::cout<<"},\"events\":[";
        for(std::size_t i=0;i<host.effects.size();++i){if(i)std::cout<<',';std::cout<<'"'<<host.effects[i]<<'"';}
        std::cout<<"],\"inputCaptureBeforeReset\":[";
        for(std::size_t i=0;i<host.captures.size();++i){if(i)std::cout<<',';const auto& c=host.captures[i];std::cout<<"{\"capturedOwner\":"<<c.owner<<",\"capturedMode\":"<<c.mode<<",\"resetReceiver\":"<<c.receiver<<'}';}
        std::cout<<"],\"clockObservations\":[";
        for(std::size_t i=0;i<host.clockObservations.size();++i)
        {if(i)std::cout<<',';const auto& c=host.clockObservations[i];std::cout<<"{\"sample\":"<<c.sample<<",\"raw64\":"<<c.raw64<<",\"globalOwner\":"<<c.owner<<",\"ownerFlags20\":";PrintNumbers(c.flags);std::cout<<",\"timerBefore\":";PrintNumbers(c.timer);std::cout<<'}';}
        std::cout<<"],\"parentTaskTimerStateUnchanged\":true}\n";
    }
    void RunEntry(std::uint32_t mode,unsigned inputByte,unsigned mutations,bool emit)
    {
        spSubscriptionManager subscriptions;EntryHost host(subscriptions);host.inputByte=inputByte;host.mutations=mutations;
        {
            wxPressStartGameFlowState object(host);host.active=&object;object.MenuStateForFixture().object4C=host.level.get();object.MenuStateForFixture().mode44=mode;
            auto& state=object.PressStartStateForFixture();state.field60=255;state.capturedWord64=0x12345678;
            std::ostringstream before;AppendTimer(before,object.TimerForAnalysis());AppendTimer(before,*object.GetMenuStateForAnalysis().timer50);
            object.FlowSlot20ForAnalysis();std::ostringstream after;AppendTimer(after,object.TimerForAnalysis());AppendTimer(after,*object.GetMenuStateForAnalysis().timer50);
            Check(before.str()==after.str()&&object.GetMenuStateForAnalysis().object4C==host.level.get(),"Entry retains exact owned level and parent timer states");
            Check(host.clockObservations.size()==2&&host.captures.size()==1,"Whole Entry foreign observations");
            if(emit)PrintEntry(object,host);
        }
        Check(!host.level&&host.AllTimersReleased()&&subscriptions.GetSubscriberCountForAnalysis(0x20)==0,"Actual owned GameLevel and parent timer/subscription cleanup, no clears4C");
    }
    void ReplayEntry()
    {
        std::uint32_t mode;unsigned inputByte,mutations;
        while(std::cin>>mode>>inputByte>>mutations){Check(inputByte<256&&mutations<512,"Entry fixture ranges");RunEntry(mode,inputByte,mutations,true);}
        Check(std::cin.eof(),"Complete Entry replay input");
    }
    void EntryContracts()
    {
        RunEntry(0xFFFFFFFF,0,159,false);RunEntry(10,255,1,false);RunEntry(0,255,384,false);
        spSubscriptionManager subscriptions;EntryHost host(subscriptions);
        {
            wxPressStartGameFlowState object(host);host.active=&object;object.MenuStateForFixture().object4C=host.level.get();
            const auto reject=[&](const char* message){bool refused=false;try{object.FlowSlot20ForAnalysis();}catch(const std::logic_error&){refused=true;}Check(refused&&host.effects.empty()&&host.captures.empty()&&host.clockObservations.empty(),message);};
            host.admitted=false;reject("Default/unqualified Entry refuses before parent effects");host.admitted=true;
            host.level->SetName("");reject("Allocated empty name remains outside native null-name profile");host.level->SetName(nullptr);
            Check(host.level->AddInstanceForAnalysis(std::make_unique<spTemplateInstance>()),"Nonempty source-only guard fixture");reject("Nonempty owned level refuses before effects");host.level->ClearInstancesForAnalysis();
            spBaseObject wrong;object.MenuStateForFixture().object4C=&wrong;reject("Wrong owned type refuses before effects");object.MenuStateForFixture().object4C=host.level.get();
        }
        Check(!host.level&&host.AllTimersReleased(),"Admission guard fixture clean retirement");
    }

    struct ExitHost final : EntryHost
    {
        unsigned exitMutations=0,resetOrdinal=0;
        bool inExit=false,exitAdmitted=true,pcExit=true,composition=false,packetValid=true;
        const void* firstBroadcastFrame=nullptr;
        bool sharedBroadcastFrame=true;
        std::vector<std::string> exitPackets,exitBoundaries,exitResets;
        explicit ExitHost(spSubscriptionManager& value):EntryHost(value) {}
        ExitProfileForAnalysis PressStartExitProfileForAnalysis() const noexcept override
        {return exitAdmitted?ExitProfileForAnalysis::PCStableMenuAndSubscriptions:ExitProfileForAnalysis::Unqualified;}
        HookProfileForAnalysis MenuHookProfileForAnalysis() const noexcept override
        {return pcExit?HookProfileForAnalysis::PC:HookProfileForAnalysis::PS2;}
        static spTimer::StateForAnalysis SeedTimer() noexcept
        {return {255,0x10203040,0x50607080,255,0x90ABCDEF};}
        std::string Snapshot() const
        {
            const auto& menu=active->GetMenuStateForAnalysis();const auto& own=active->GetPressStartStateForAnalysis();
            Check(own.capturedWord64.has_value(),"Explicit captured64 or actual Entry known64");std::ostringstream out;
            out<<"{\"view\":"<<menu.view40<<",\"mode\":"<<menu.mode44<<",\"field25\":"<<unsigned(active->GetStateForAnalysis().field25)
                <<",\"field60\":"<<unsigned(own.field60)<<",\"raw64\":"<<*own.capturedWord64<<",\"cachedInputID\":"<<Number(own.borrowedInput5C)
                <<",\"owner\":"<<owner<<",\"ownerMembers1C\":["<<ownerMembers[0]<<','<<ownerMembers[1]
                <<"],\"ownerFlags20\":["<<unsigned(ownerFlags[0])<<','<<unsigned(ownerFlags[1])<<"],\"embeddedTimer\":[";
            const auto values=TimerState(active->EmbeddedTimerForAnalysis());
            for(std::size_t i=0;i<values.size();++i){if(i)out<<',';out<<values[i];}
            out<<"],\"globalGUI\":"<<gui<<",\"globalDX\":"<<dx<<",\"shared45\":";
            if(composition)out<<unsigned(sharedFlags[shared-1]);else out<<"null";out<<'}';return out.str();
        }
        std::string Boundary(const char* kind,unsigned receiver,const std::vector<unsigned>& arguments,
                             const std::string& before) const
        {
            std::ostringstream out;out<<"{\"kind\":\""<<kind<<"\",\"receiver\":"<<receiver<<",\"arguments\":[";
            for(std::size_t i=0;i<arguments.size();++i){if(i)out<<',';out<<arguments[i];}
            out<<"],\"before\":"<<before<<",\"after\":"<<Snapshot()<<'}';return out.str();
        }
        void Packet(const spNotificationForAnalysis& packet,const std::string& kind)
        {
            const std::array<std::uintptr_t,8> words{packet.code,packet.word04,packet.word08,packet.word0C,
                packet.sender==active?1u:0u,packet.word14,packet.payload,packet.tail};
            packetValid=packetValid&&packet.sender==active;
            const auto before=Snapshot();std::ostringstream row,event;row<<"{\"receiver\":\""<<kind<<"\",\"logicalWords\":[";event<<kind;
            for(std::size_t i=0;i<words.size();++i){if(i)row<<',';row<<words[i];event<<':'<<words[i];}
            effects.push_back(event.str());
            if(kind=="D1"&&(exitMutations&1))
            {active->MenuStateForFixture().view40=7;active->MutableStateForAnalysis().field25=0xEE;gui=dx=owner=2;}
            if(kind=="N1"&&(exitMutations&128))
            {auto& state=active->PressStartStateForFixture();state.field60=0xBB;state.capturedWord64=0xCAFEBABE;state.borrowedInput5C=Token(2);owner=2;}
            if(kind=="N1"&&(exitMutations&256))
            {auto& value=*const_cast<spNotificationForAnalysis*>(&packet);value.code=0xFE123456;value.payload=0xFFFFFFFF;value.tail=0x80000000;}
            row<<"],\"before\":"<<before<<",\"after\":"<<Snapshot()<<'}';exitPackets.push_back(row.str());
            if(kind=="N1")firstBroadcastFrame=&packet;
            if(kind=="N2")sharedBroadcastFrame=sharedBroadcastFrame&&firstBroadcastFrame==&packet;
        }
        void DispatchNotificationForAnalysis(void* target,const spNotificationForAnalysis& packet) override
        {
            if(!inExit){Host::DispatchNotificationForAnalysis(target,packet);return;}
            Check(target==Token(200),"Exit current directed receiver");Packet(packet,"D1");
        }
        void SetMenuViewEnabledForAnalysis(void* value,std::uint32_t view,std::uint32_t enabled) override
        {
            if(!inExit){EntryHost::SetMenuViewEnabledForAnalysis(value,view,enabled);return;}
            const auto id=Number(value)-400;Check(id==gui&&view==active->GetMenuStateForAnalysis().view40&&enabled==0,"Exit fresh GUI/current view");
            const auto before=Snapshot();effects.push_back("V"+std::to_string(id)+':'+std::to_string(view)+":0");
            if(exitMutations&2)active->MutableStateForAnalysis().field25=0xAA;
            exitBoundaries.push_back(Boundary("View",id,{view,enabled},before));
        }
        void SetMenuHUDForAnalysis(void* value,std::uint32_t enabled) override
        {
            if(!inExit){EntryHost::SetMenuHUDForAnalysis(value,enabled);return;}
            const auto id=Number(value)-400;Check(id==gui&&enabled==0,"Exit HUD resolves GUI again");const auto before=Snapshot();effects.push_back("H"+std::to_string(id)+":0");
            if(exitMutations&8)active->MutableStateForAnalysis().field25=0xBB;
            exitBoundaries.push_back(Boundary("HUD",id,{enabled},before));
        }
        void* GetMenuAudioControllerForAnalysis(void* value) override
        {
            if(!inExit)return EntryHost::GetMenuAudioControllerForAnalysis(value);
            const auto id=Number(value)-500;Check(id==dx,"Fresh confirmed DXInput embedded view18");const auto before=Snapshot();effects.push_back("DX"+std::to_string(id)+":Get");
            exitBoundaries.push_back(Boundary("DXGet",id,{},before));return Token(600+id);
        }
        void SetMenuAudioEnabledForAnalysis(void* value,std::uint32_t enabled) override
        {
            if(!inExit){EntryHost::SetMenuAudioEnabledForAnalysis(value,enabled);return;}
            const auto id=Number(value)-600;Check(id==dx&&enabled==0&&active->GetStateForAnalysis().field25==0,"Parent clear25 precedes DXInput Set0");
            const auto before=Snapshot();effects.push_back("DX"+std::to_string(id)+":Set:0");
            if(exitMutations&4){gui=2;active->MutableStateForAnalysis().field25=0xEE;}
            exitBoundaries.push_back(Boundary("DXSet",id,{enabled},before));
        }
        void* ResolveMenuExitServiceForAnalysis() override{Check(inExit,"Foreign Exit service only in Exit");return Token(801);}
        void InvokeMenuExitServiceForAnalysis(void* value,std::uint32_t argument) override
        {
            Check(inExit&&value==Token(801)&&argument==1,"Opaque service4628A0 receiver/value");const auto before=Snapshot();effects.push_back("X1:1");
            if(exitMutations&16)owner=2;exitBoundaries.push_back(Boundary("ExitService",1,{argument},before));
        }
        void ResetMenuInputForAnalysis(void* value) override
        {
            if(!inExit){EntryHost::ResetMenuInputForAnalysis(value);return;}
            const auto id=Number(value)-10;Check(id==ownerMembers[owner-1]&&resetOrdinal<2,"Fresh current owner/member for each Exit Reset");
            const bool own=resetOrdinal++==1;const auto kind=own?"own":"parent";const auto before=Snapshot();effects.push_back("R"+std::to_string(id)+':'+kind);
            if(!own&&(exitMutations&32)){const auto captured=owner;ownerMembers[captured-1]=2*captured;owner=3-captured;}
            if(own&&(exitMutations&64))
            {auto& state=active->PressStartStateForFixture();state.field60=0xDD;state.capturedWord64=0xAABBCCDD;active->EmbeddedTimerForFixture().SetStateForAnalysis(SeedTimer());}
            exitResets.push_back("{\"kind\":\""+std::string(kind)+"\",\"receiver\":"+std::to_string(id)+",\"before\":"+before+",\"after\":"+Snapshot()+'}');
        }
        void SetMenuInputModeForAnalysis(void* value,std::uint32_t mode) override
        {Check(!inExit,"Exit must not call SetMode");EntryHost::SetMenuInputModeForAnalysis(value,mode);}
    };
    struct ExitReceiver final : spBaseObject
    {
        ExitHost* host=nullptr;unsigned id=0;
        void vfunc_0C(const void* value) noexcept override
        {host->Packet(*static_cast<const spNotificationForAnalysis*>(value),"N"+std::to_string(id));}
    };
    void PrintJsonRows(const std::vector<std::string>& rows)
    {std::cout<<'[';for(std::size_t i=0;i<rows.size();++i){if(i)std::cout<<',';std::cout<<rows[i];}std::cout<<']';}
    void RunExit(unsigned owned,unsigned seededActive,unsigned target,unsigned mutations,unsigned composition,bool emit)
    {
        spSubscriptionManager subscriptions;ExitHost host(subscriptions);std::array<ExitReceiver,2> receivers;
        for(unsigned i=0;i<2;++i){receivers[i].host=&host;receivers[i].id=i+1;Check(subscriptions.SubscribeForAnalysis(0x11,receivers[i]),"Real source key11 subscribe");}
        if(!owned)host.level.reset();host.targetPresent=target;host.composition=composition!=0;
        {
            wxPressStartGameFlowState object(host);host.active=&object;
            if(owned)object.MenuStateForFixture().object4C=host.level.get();
            auto& own=object.PressStartStateForFixture();own.field60=composition?255:0xA5;own.capturedWord64=0x12345678;
            if(composition)
            {object.FlowSlot20ForAnalysis();Check(host.clockObservations.size()==2&&host.captures.size()==1,"Actual source Entry before Exit composition");}
            else object.MutableStateForAnalysis().field25=0xA5;
            if(seededActive)object.EmbeddedTimerForFixture().SetStateForAnalysis(ExitHost::SeedTimer());
            host.effects.clear();host.inExit=true;host.exitMutations=mutations;
            const auto before=host.Snapshot();const auto sampleCount=host.clockObservations.size();
            std::ostringstream parentBefore;AppendTimer(parentBefore,object.TimerForAnalysis());AppendTimer(parentBefore,*object.GetMenuStateForAnalysis().timer50);
            object.FlowSlot24ForAnalysis();
            std::ostringstream parentAfter;AppendTimer(parentAfter,object.TimerForAnalysis());AppendTimer(parentAfter,*object.GetMenuStateForAnalysis().timer50);
            Check(parentBefore.str()==parentAfter.str()&&sampleCount==host.clockObservations.size(),"Exit preserves real parent timers and makes no clock observation");
            Check(host.packetValid&&host.sharedBroadcastFrame&&host.resetOrdinal==2&&host.exitPackets.size()==(target?3u:2u),"Source actual shared fanout/frame/input observations");
            Check(object.GetMenuStateForAnalysis().object4C==host.level.get(),"Exit retains exact owned4C until actual parent destruction");
            if(emit)
            {
                std::cout<<"{\"beforeExit\":"<<before<<",\"afterExit\":"<<host.Snapshot()<<",\"events\":[";
                for(std::size_t i=0;i<host.effects.size();++i){if(i)std::cout<<',';std::cout<<'"'<<host.effects[i]<<'"';}
                std::cout<<"],\"packets\":";PrintJsonRows(host.exitPackets);std::cout<<",\"boundaries\":";PrintJsonRows(host.exitBoundaries);
                std::cout<<",\"resets\":";PrintJsonRows(host.exitResets);std::cout<<",\"sharedBroadcastFrame\":true,\"parentTimersUnchanged\":true,\"clockCountBeforeExit\":"<<sampleCount
                    <<",\"clockCountAfterExit\":"<<host.clockObservations.size()<<"}\n";
            }
        }
        Check(!host.level&&host.AllTimersReleased()&&subscriptions.GetSubscriberCountForAnalysis(0x20)==0,"Real selected owned object/timers/subscription cleanup; no clear4C");
        for(auto& receiver:receivers)Check(subscriptions.UnsubscribeForAnalysis(0x11,receiver),"Actual shared key11 removal");
        Check(subscriptions.GetSubscriberCountForAnalysis(0x11)==0,"No selected receiver retained");
    }
    void ReplayExit()
    {
        unsigned owned,active,target,mutations,composition;
        while(std::cin>>owned>>active>>target>>mutations>>composition)
        {Check(owned<2&&active<2&&target<2&&mutations<512&&composition<2&&(!composition||owned),"Five bounded Exit replay inputs");RunExit(owned,active,target,mutations,composition,true);}
        Check(std::cin.eof(),"Complete Exit replay input");
    }
    void ExitContracts()
    {
        RunExit(0,0,1,0,0,false);RunExit(1,1,1,511,0,false);RunExit(1,0,1,0,1,false);RunExit(0,1,0,0,0,false);
        spSubscriptionManager subscriptions;ExitHost host(subscriptions);
        {
            wxPressStartGameFlowState object(host);host.active=&object;object.MenuStateForFixture().object4C=host.level.get();
            object.PressStartStateForFixture().capturedWord64=0x12345678;host.inExit=true;
            const auto reject=[&](const char* message){bool refused=false;try{object.FlowSlot24ForAnalysis();}catch(const std::logic_error&){refused=true;}
                Check(refused&&host.effects.empty()&&host.exitResets.empty()&&host.exitPackets.empty(),message);};
            host.exitAdmitted=false;reject("Unqualified Exit refuses before parent effects");host.exitAdmitted=true;
            host.pcExit=false;reject("PS2 Exit remains outside PC admission");host.pcExit=true;
            host.level->SetName("");reject("Nonnull allocated empty level name refuses");host.level->SetName(nullptr);
            Check(host.level->AddInstanceForAnalysis(std::make_unique<spTemplateInstance>()),"Nonempty Exit guard fixture");reject("Nonempty level refuses before parent effects");host.level->ClearInstancesForAnalysis();
            spBaseObject wrong;object.MenuStateForFixture().object4C=&wrong;reject("Wrong owned type refuses before effects");object.MenuStateForFixture().object4C=host.level.get();
            {spSubscriptionManager temporary;}reject("Missing existing shared manager refuses before parent effects");
        }
        Check(!host.level&&host.AllTimersReleased(),"Exit admission fixture owned cleanup");
    }


}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2 && std::string(argv[1])=="--replay")Replay();
        else if(argc==2 && std::string(argv[1])=="--entry-replay")ReplayEntry();
        else if(argc==2 && std::string(argv[1])=="--exit-replay")ReplayExit();
        else if(argc==1) {Contracts();EntryContracts();ExitContracts();std::cout<<"PressStart contracts passed\n";}
        else return 1;
        return 0;
    }
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
