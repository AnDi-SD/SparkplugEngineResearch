#include "Code/wxCreditsMenuGameFlowState.h"
#include "Analysis/PC/wxCreditsMenuGameFlowStateAbi.h"
#include "Analysis/PS2/wxCreditsMenuGameFlowStateAbi.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
    void* Token(std::uintptr_t value) { return reinterpret_cast<void*>(value); }
    std::string Join(const std::vector<std::string>& values)
    {
        std::string result;
        for(const auto& value:values) { if(!result.empty())result+=',';result+=value; }
        return result.empty()?"-":result;
    }
    struct Host final : wxCreditsMenuGameFlowStateHost
    {
        wxGameFlowResourcesForAnalysis resources;
        std::vector<std::string> effects;
        std::set<wxCreditsMenuGameFlowState*> listeners;
        wxCreditsMenuGameFlowState* active=nullptr;
        std::uint32_t mutations=0,controllerState=0x4A,borrowCount=0;
        std::array<std::uint8_t,2> samples{0,0};
        std::size_t sampleIndex=0;
        std::uint8_t alternateInput=1,shared45=0xA5;
        std::array<std::uint8_t,2> owner20{0xA5,0xA5};
        void* currentBorrowed=Token(0x3C);
        void* currentOwner=Token(0x100);
        void* firstOwnerInput=Token(1);
        void* currentController=Token(0x1000);
        void* currentService=Token(0x3000);
        void* currentHUD=Token(0x8000);
        std::vector<void*> borrowedSequence;
        bool missingInput=false;

        unsigned ControllerID(void* value)
        { Check(value==Token(0x1000)||value==Token(0x2000),"Controller token");return value==Token(0x1000)?1:2; }
        void* BorrowInputForAnalysis() override
        {
            const auto index=borrowCount++;
            return index<borrowedSequence.size()?borrowedSequence[index]:currentBorrowed;
        }
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override { return {1200,1}; }
        wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() override { return resources; }
        void RegisterCreditsReceiverForAnalysis(wxCreditsMenuGameFlowState& object,std::uint32_t tag) override
        { Check(tag==0x13&&listeners.insert(&object).second,"Credits listener registration");active=&object; }
        void UnregisterCreditsReceiverForAnalysis(wxCreditsMenuGameFlowState& object,std::uint32_t tag) override
        { Check(tag==0x13&&listeners.erase(&object)==1,"Credits listener unregistration");if(active==&object)active=nullptr; }
        void* ResolveCreditsAudioInterfaceForAnalysis() override { return Token(0x6000); }
        void* GetCreditsAudioControllerForAnalysis(void* audio) override
        { Check(audio==Token(0x6000),"Audio interface receiver");effects.push_back("A");return Token(0x7000); }
        void SetCreditsAudioEnabledForAnalysis(void* controller,std::uint32_t value) override
        {
            Check(controller==Token(0x7000),"Audio controller receiver");effects.push_back("E:"+std::to_string(value));
            if(mutations&128) { currentHUD=Token(0x9000);shared45=0x5A; }
        }
        std::uint8_t& BorrowCreditsSharedFlag45ForAnalysis() override { effects.push_back("F");return shared45; }
        void* ResolveCreditsHUDForAnalysis() override { return currentHUD; }
        void InvokeCreditsHUDForAnalysis(void* hud,std::uint32_t value) override
        { Check(hud==Token(0x8000)||hud==Token(0x9000),"HUD receiver");effects.push_back(std::string(hud==Token(0x8000)?"H1:":"H2:")+std::to_string(value)); }
        void* ResolveHookInputOwnerForAnalysis() override { return currentOwner; }
        void* ReadHookInputForAnalysis(void* owner) override
        {
            if(missingInput)return nullptr;
            Check(owner==Token(0x100)||owner==Token(0x200),"Hook input owner");
            return owner==Token(0x100)?firstOwnerInput:Token(3);
        }
        void ResetHookInputForAnalysis(void* input) override
        {
            effects.push_back("R"+std::to_string(reinterpret_cast<std::uintptr_t>(input)));
            if(mutations&1)firstOwnerInput=Token(2);
            if(mutations&2)currentOwner=Token(0x200);
        }
        void SetHookInputModeForAnalysis(void* input,std::uint32_t mode) override
        {
            effects.push_back("M"+std::to_string(reinterpret_cast<std::uintptr_t>(input))+":"+std::to_string(mode));
            if((mutations&64)&&active)active->CreditsStateForFixture().field41=0;
        }
        std::uint8_t ReadCreditsCachedInput2AForAnalysis(void* input) override
        {
            Check(input==Token(0x3C)||input==Token(0x3D),"Cached Credits input");
            const auto value=input==Token(0x3D)?alternateInput:samples[std::min(sampleIndex++,samples.size()-1)];
            effects.push_back(std::string(input==Token(0x3C)?"I1:":"I2:")+std::to_string(value));return value;
        }
        std::uint8_t& BorrowCreditsInputOwnerFlag20ForAnalysis(void* owner) override
        {
            Check(owner==Token(0x100)||owner==Token(0x200),"Owner flag20 receiver");
            effects.push_back(owner==Token(0x100)?"B1":"B2");return owner20[owner==Token(0x100)?0:1];
        }
        void* ResolveCreditsControllerForAnalysis() override { return currentController; }
        std::uint32_t ReadCreditsControllerState1B0ForAnalysis(void* controller) override
        { effects.push_back("S"+std::to_string(ControllerID(controller))+":"+std::to_string(controllerState));return controllerState; }
        void RequestCreditsControllerStateForAnalysis(void* controller,std::uint32_t state,std::uint32_t parameter) override
        {
            effects.push_back("Q"+std::to_string(ControllerID(controller))+":"+std::to_string(state)+":"+std::to_string(parameter));
            if(mutations&8)currentController=Token(0x2000);
            if((mutations&512)&&active) { active->CreditsStateForFixture().field40=0xC7;active->CreditsStateForFixture().field41=0xEE; }
        }
        void SetCreditsControllerStateForAnalysis(void* controller,std::uint32_t state,std::uint32_t parameter) override
        {
            effects.push_back("C"+std::to_string(ControllerID(controller))+":"+std::to_string(state)+":"+std::to_string(parameter));
            if(mutations&16)currentService=Token(0x4000);
        }
        void* ResolveCreditsServiceForAnalysis() override { return currentService; }
        void SendCreditsServiceEventForAnalysis(void* service,std::uint32_t event,std::uint32_t parameter) override
        {
            Check(service==Token(0x3000)||service==Token(0x4000),"Service receiver");
            effects.push_back(std::string(service==Token(0x3000)?"T1:":"T2:")+std::to_string(event)+":"+std::to_string(parameter));
            if(mutations&32)currentOwner=Token(0x200);
        }
        void* ResolveCreditsOnceServiceForAnalysis() override { return Token(0x5000); }
        void SendCreditsOnceRequestForAnalysis(void* service,std::uint32_t value) override
        {
            Check(service==Token(0x5000)&&active&&active->GetCreditsStateForAnalysis().field41==0,"Once clear precedes callback");
            effects.push_back("O"+std::to_string(value));
            if(mutations&4)
            { active->CreditsStateForFixture().field40=1;active->CreditsStateForFixture().field41=0xA5;active->CreditsStateForFixture().cachedInput3C=Token(0x3D); }
        }
    };
    unsigned CacheID(const wxCreditsMenuGameFlowState& object)
    { return object.GetCreditsStateForAnalysis().cachedInput3C==Token(0x3C)?1:object.GetCreditsStateForAnalysis().cachedInput3C==Token(0x3D)?2:0; }
    void Emit()
    {
        std::string line;
        while(std::getline(std::cin,line))
        {
            std::istringstream input(line);std::string op;
            unsigned flag40,flag41,a,b;std::uint32_t code,payload;Host host;
            if(!(input>>op>>flag40>>flag41>>a>>b>>host.controllerState>>host.mutations>>code>>payload)
                ||flag40>255||flag41>255||a>255||b>255)throw std::invalid_argument("Credits case inputs");
            host.samples={static_cast<std::uint8_t>(a),static_cast<std::uint8_t>(b)};
            wxCreditsMenuGameFlowState object(host);object.CreditsStateForFixture().field40=static_cast<std::uint8_t>(flag40);
            object.CreditsStateForFixture().field41=static_cast<std::uint8_t>(flag41);host.effects.clear();bool result=true;
            if(op=="tick")result=object.FlowSlot30ForAnalysis();
            else if(op=="enter")object.FlowSlot20ForAnalysis();
            else if(op=="exit")object.FlowSlot2CForAnalysis();
            else if(op=="complete")object.CompleteCreditsForAnalysis();
            else if(op=="notify") { spNotificationForAnalysis message;message.code=code;message.payload=payload;object.NotifyCreditsForAnalysis(message); }
            else throw std::invalid_argument("Unknown Credits operation");
            const auto& state=object.GetCreditsStateForAnalysis();
            std::cout<<result<<' '<<unsigned(state.field40)<<' '<<unsigned(state.field41)<<' '<<CacheID(object)
                <<' '<<unsigned(host.shared45)<<' '<<unsigned(host.owner20[0])<<' '<<unsigned(host.owner20[1])<<' '<<Join(host.effects)<<'\n';
        }
    }
    void TestConstructorCopyClone()
    {
        Host host;host.borrowedSequence={Token(0x18),Token(0x3C)};
        {
            wxCreditsMenuGameFlowState source(host);
            Check(host.borrowCount==2&&source.GetBorrowedInputForAnalysis()==Token(0x18)
                &&source.GetCreditsStateForAnalysis().cachedInput3C==Token(0x3C),"Credits own input uses a second constructor read");
            Check(source.GetCreditsStateForAnalysis().field40==0&&source.GetCreditsStateForAnalysis().field41==1,"Native own defaults00/01");
            source.CreditsStateForFixture().field40=0xA5;source.CreditsStateForFixture().field41=0xFE;
            wxCreditsMenuGameFlowState target(host);target.CreditsStateForFixture().field40=17;target.CreditsStateForFixture().field41=19;
            const auto oldInput=target.GetCreditsStateForAnalysis().cachedInput3C;auto* timer=&target.TimerForAnalysis();
            spCloneManager manager;Check(source.vfunc_14(target,manager),"Inherited Copy returns true");
            Check(target.GetCreditsStateForAnalysis().field40==17&&target.GetCreditsStateForAnalysis().field41==19
                &&target.GetCreditsStateForAnalysis().cachedInput3C==oldInput&& &target.TimerForAnalysis()==timer,"Copy preserves destination state/input/timer");
            host.currentBorrowed=Token(0x3D);auto cloned=manager.Clone(source);auto* typed=dynamic_cast<wxCreditsMenuGameFlowState*>(cloned.get());
            Check(typed&&typed->GetCreditsStateForAnalysis().field40==0&&typed->GetCreditsStateForAnalysis().field41==1
                &&typed->GetCreditsStateForAnalysis().cachedInput3C==Token(0x3D)&& &typed->TimerForAnalysis()!=&source.TimerForAnalysis(),"Clone uses new constructor defaults/binding/timer");
            Check(manager.FindClone(source)==nullptr&&host.listeners.size()==3,"Clone registration transaction and listener ownership");
            wxCreditsMenuGameFlowState::SetFactoryHostForAnalysis(&host);
            auto factory=spRTTIManager::Instance().Create(wxCreditsMenuGameFlowState::ClassID);
            wxCreditsMenuGameFlowState::SetFactoryHostForAnalysis(nullptr);
            Check(factory&&factory->IsKindOf(wxGameFlowState::ClassID),"Registered Credits factory and physical base");
        }
        Check(host.listeners.empty(),"All constructed Credits listeners unregistered");
    }
    void TestFlagMachineAndReloads()
    {
        Host host;wxCreditsMenuGameFlowState object(host);host.effects.clear();
        Check(object.FlowSlot30ForAnalysis()&&Join(host.effects)=="O80,I1:0"
            &&object.GetCreditsStateForAnalysis().field40==1&&object.GetCreditsStateForAnalysis().field41==0,"First tick once request and release arm");
        host.effects.clear();host.sampleIndex=0;host.samples={1,1};object.FlowSlot30ForAnalysis();
        Check(Join(host.effects)=="I1:1,S1:74,Q1:60:0,C1:56:0,T1:700:0,B1"
            &&object.GetCreditsStateForAnalysis().field40==0&&host.owner20[0]==1,"Rising input completes in native order");
        host.effects.clear();host.sampleIndex=0;object.FlowSlot30ForAnalysis();
        Check(Join(host.effects)=="I1:1"&&object.GetCreditsStateForAnalysis().field40==0,"Held input does not retrigger");
        object.CreditsStateForFixture().field40=0x88;host.effects.clear();host.sampleIndex=0;host.samples={0,1};object.FlowSlot30ForAnalysis();
        Check(Join(host.effects)=="I1:0,I1:1"&&object.GetCreditsStateForAnalysis().field40==0x88,"Two input reads preserve raw armed byte on changed second value");
        host.mutations=4|8|16|32|512;object.CreditsStateForFixture().field40=0;object.CreditsStateForFixture().field41=1;
        host.effects.clear();object.FlowSlot30ForAnalysis();
        Check(Join(host.effects)=="O80,I2:1,S1:74,Q1:60:0,C2:56:0,T2:700:0,B2"
            &&object.GetCreditsStateForAnalysis().field40==0xC7&&object.GetCreditsStateForAnalysis().field41==0xEE
            &&host.owner20[1]==1,"Foreign callbacks can replace cached input/controller/service/owner after preceding stores");
    }
    void TestEntryExitAndNotify()
    {
        Host host;wxCreditsMenuGameFlowState object(host);host.effects.clear();host.mutations=1|2|64|128;
        object.CreditsStateForFixture().field40=0xA7;object.CreditsStateForFixture().field41=0;
        object.FlowSlot20ForAnalysis();
        Check(Join(host.effects)=="A,E:1,F,H2:0,R1,M2:0"&&host.shared45==0
            &&object.GetCreditsStateForAnalysis().field40==0xA7&&object.GetCreditsStateForAnalysis().field41==1,"Entry order, input reload, final once store");
        host.currentOwner=Token(0x100);host.firstOwnerInput=Token(1);host.effects.clear();object.FlowSlot2CForAnalysis();
        Check(Join(host.effects)=="R1,M2:0"&&object.GetCreditsStateForAnalysis().field41==0,"Exit does not undo callback flag mutation");
        host.effects.clear();host.mutations=0;spNotificationForAnalysis packet;packet.code=0x27DB;packet.payload=0x4F;
        object.NotifyCreditsForAnalysis(packet);Check(host.effects.empty(),"Unmatched Credits payload ignored");
        packet.code=0x27DA;packet.payload=std::numeric_limits<std::uintptr_t>::max();object.NotifyCreditsForAnalysis(packet);
        Check(host.effects.empty(),"Other notification code uses base no-op without reading numeric payload domain");
        packet.code=0x27DB;packet.payload=0x50;object.CreditsStateForFixture().field40=0xA7;
        object.vfunc_0C(&packet);
        Check(Join(host.effects)=="S1:74,Q1:60:0,C1:56:0,T1:700:0,B2"
            &&object.GetCreditsStateForAnalysis().field40==0xA7,"Notification invokes completion without Tick's clear40");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--emit") { Emit();return 0; }
        TestConstructorCopyClone();TestFlagMachineAndReloads();TestEntryExitAndNotify();
        std::cout<<"PASS Credits entry/exit/tick/completion/notification, callback reloads and clone lifetime\n";return 0;
    }
    catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n';return 1; }
}
