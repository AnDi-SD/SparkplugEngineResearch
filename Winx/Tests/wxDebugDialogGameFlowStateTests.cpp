#include "Code/wxDebugMenuGameFlowState.h"
#include "Code/wxDialogWindowGameFlowState.h"
#include "Analysis/PC/wxDebugMenuGameFlowStateAbi.h"
#include "Analysis/PS2/wxDebugMenuGameFlowStateAbi.h"
#include "Analysis/PC/wxDialogWindowGameFlowStateAbi.h"
#include "Analysis/PS2/wxDialogWindowGameFlowStateAbi.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(const bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
    void* Token(std::uintptr_t value) { return reinterpret_cast<void*>(value); }
    std::string Join(const std::vector<std::string>& effects)
    {
        std::string value;
        for(const auto& effect:effects) { if(!value.empty())value+=',';value+=effect; }
        return value.empty()?"-":value;
    }
    struct DebugHost final : wxDebugMenuGameFlowStateHost
    {
        wxGameFlowResourcesForAnalysis resources;
        std::vector<std::string> effects;
        std::uint32_t flags=0,resolvedServices=0,parentConstructions=0;
        void* currentOwner=Token(0x100);
        void* firstOwnerInput=Token(1);
        void* currentService=Token(10);
        bool missingInput=false;
        void* BorrowInputForAnalysis() override { ++parentConstructions;return Token(0x18); }
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override { return {1200,1}; }
        wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() override { return resources; }
        void* ResolveHookInputOwnerForAnalysis() override { return currentOwner; }
        void* ReadHookInputForAnalysis(void* owner) override
        {
            if(missingInput)return nullptr;
            Check(owner==Token(0x100)||owner==Token(0x200),"Captured input owner");
            return owner==Token(0x100)?firstOwnerInput:Token(3);
        }
        void ResetHookInputForAnalysis(void* input) override
        {
            effects.push_back("R"+std::to_string(reinterpret_cast<std::uintptr_t>(input)));
            if(flags&1)firstOwnerInput=Token(2);
            if(flags&2)currentOwner=Token(0x200);
            if(flags&4)currentService=Token(11);
        }
        void SetHookInputModeForAnalysis(void* input,std::uint32_t mode) override
        {
            effects.push_back("M"+std::to_string(reinterpret_cast<std::uintptr_t>(input))+":"+std::to_string(mode));
            if(flags&8)currentService=Token(11);
        }
        void* ResolveDebugServiceForAnalysis() override { ++resolvedServices;return currentService; }
        void Consumer(void* service,const char* kind)
        {
            Check(service==Token(10)||service==Token(11),"Debug service receiver");
            effects.push_back(std::string(kind)+(service==Token(10)?":1":":2"));
        }
        void InvokeDebugSlot20ConsumerForAnalysis(void* service) override { Consumer(service,"D20"); }
        void InvokeDebugSlot24ConsumerForAnalysis(void* service) override { Consumer(service,"D24"); }
    };
    struct GuardedWindow final
    {
        std::array<std::uint8_t,0x14> prefix;
        wxDialogWindowGameFlowStateHost::ResetRegionForAnalysis region;
        std::array<std::uint8_t,0x14> suffix;
        void Fill(std::uint8_t value) { prefix.fill(value);region.fill(value);suffix.fill(value); }
    };
    static_assert(sizeof(GuardedWindow)==0x280);
    static_assert(offsetof(GuardedWindow,region)==0x14);
    static_assert(offsetof(GuardedWindow,suffix)==0x26C);
    struct DialogHost final : wxDialogWindowGameFlowStateHost
    {
        wxGameFlowResourcesForAnalysis resources;
        GuardedWindow window;
        std::vector<std::string> effects;
        bool missingWindow=false;
        std::uint32_t parentConstructions=0;
        DialogHost() { window.Fill(0xA5); }
        void* BorrowInputForAnalysis() override { ++parentConstructions;return Token(0x18); }
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override { return {1200,1}; }
        wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() override { return resources; }
        void* ResolveDialogWindowForAnalysis() override { return missingWindow?nullptr:Token(1); }
        ResetRegionForAnalysis& BorrowDialogWindowResetRegionForAnalysis(void* receiver) override
        { Check(receiver==Token(1),"Window storage receiver");return window.region; }
        void InvokeDialogWindowConsumerForAnalysis(void* receiver) override
        { Check(receiver==Token(1),"Window consumer receiver");effects.push_back("W1"); }
    };
    std::string Hex(const GuardedWindow& window)
    {
        static constexpr char digits[]="0123456789ABCDEF";
        const auto* bytes=reinterpret_cast<const unsigned char*>(&window);
        std::string result;result.reserve(sizeof(window)*2);
        for(std::size_t i=0;i<sizeof(window);++i)
        { result+=digits[bytes[i]>>4];result+=digits[bytes[i]&15]; }
        return result;
    }
    void Emit()
    {
        std::string line;
        while(std::getline(std::cin,line))
        {
            std::istringstream input(line);
            std::string kind,platform;
            std::uint32_t hook,flags,fill;
            if(!(input>>kind>>platform>>hook>>flags>>fill)||(platform!="PC"&&platform!="PS2")||fill>255)
                throw std::invalid_argument("Expected debug/dialog PC/PS2 hook flags fill");
            if(kind=="debug")
            {
                if(hook!=20&&hook!=24)throw std::invalid_argument("Debug hook20/24");
                DebugHost host;host.flags=flags;
                const auto profile=platform=="PC"?wxDebugMenuGameFlowState::HookPlatformForAnalysis::PC
                    :wxDebugMenuGameFlowState::HookPlatformForAnalysis::PS2;
                wxDebugMenuGameFlowState object(host,profile);
                if(hook==20)object.FlowSlot20ForAnalysis();else object.FlowSlot24ForAnalysis();
                std::cout<<"1 "<<Join(host.effects)<<" -\n";
            }
            else if(kind=="dialog")
            {
                if(hook!=24&&hook!=30)throw std::invalid_argument("Dialog hook24/30");
                DialogHost host;host.window.Fill(static_cast<std::uint8_t>(fill));
                wxDialogWindowGameFlowState object(host);bool result=true;
                if(hook==24)object.FlowSlot24ForAnalysis();else result=object.FlowSlot30ForAnalysis();
                std::cout<<result<<' '<<Join(host.effects)<<' '<<Hex(host.window)<<'\n';
            }
            else throw std::invalid_argument("Unknown game-flow leaf");
        }
    }
    void TestDebug()
    {
        DebugHost host;host.flags=15;
        wxDebugMenuGameFlowState object(host);
        object.FlowSlot20ForAnalysis();
        Check(Join(host.effects)=="R1,M2:7,D20:2","PC Debug captures owner, reloads input, resolves service after callbacks");
        Check(object.GetBorrowedInputForAnalysis()==Token(0x18),"Debug leaves cached parent input alone");
        Check(host.currentOwner==Token(0x200)&&host.resolvedServices==1,"Only one debug service observation");
        DebugHost ps2;ps2.flags=15;
        wxDebugMenuGameFlowState other(ps2,wxDebugMenuGameFlowState::HookPlatformForAnalysis::PS2);
        other.FlowSlot20ForAnalysis();
        Check(Join(ps2.effects)=="R1,M2:7"&&ps2.resolvedServices==0,"PS2 Debug has no PC consumer");
        DebugHost exit;exit.flags=4;
        wxDebugMenuGameFlowState closing(exit);closing.FlowSlot24ForAnalysis();
        Check(Join(exit.effects)=="R1,D24:2","Debug slot24 resolves service after reset");
        object.SetCodesForAnalysis(1,2,3);spCloneManager manager;
        auto clone=manager.Clone(object);auto* typed=dynamic_cast<wxDebugMenuGameFlowState*>(clone.get());
        Check(typed&&typed->GetStateForAnalysis().codes==std::array<std::uint32_t,3>{0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF},
            "Debug clone uses fresh parent defaults");
        Check(typed->GetHookPlatformForAnalysis()==object.GetHookPlatformForAnalysis()
            && &typed->TimerForAnalysis()!=&object.TimerForAnalysis(),"Debug clone profile and independent timer");
        DebugHost guard;guard.missingInput=true;wxDebugMenuGameFlowState guarded(guard);bool refused=false;
        try { guarded.FlowSlot20ForAnalysis(); }catch(const std::logic_error&) { refused=true; }
        Check(refused&&guard.effects.empty(),"Missing Debug input refuses before callbacks");
    }
    void TestDialog()
    {
        for(const auto fill:{0u,1u,0xA5u,0xFFu})
        {
            DialogHost host;host.window.Fill(static_cast<std::uint8_t>(fill));
            wxDialogWindowGameFlowState object(host);object.FlowSlot24ForAnalysis();
            Check(std::all_of(host.window.region.begin(),host.window.region.end(),[](auto value){return value==0;}),
                "Dialog clears exact borrowed reset region");
            Check(std::all_of(host.window.prefix.begin(),host.window.prefix.end(),[fill](auto value){return value==fill;})
                &&std::all_of(host.window.suffix.begin(),host.window.suffix.end(),[fill](auto value){return value==fill;}),
                "Dialog preserves bytes before14 and after26C");
            Check(host.effects.empty(),"Dialog reset has no input/debug/consumer callback");
            const auto before=Hex(host.window);
            Check(object.FlowSlot30ForAnalysis()&&Join(host.effects)=="W1"&&Hex(host.window)==before,
                "Dialog invokes required consumer then returns true");
        }
        DialogHost host;wxDialogWindowGameFlowState object(host);object.SetCodesForAnalysis(1,2,3);
        spCloneManager manager;auto clone=manager.Clone(object);auto* typed=dynamic_cast<wxDialogWindowGameFlowState*>(clone.get());
        Check(typed&&typed->GetStateForAnalysis().codes==std::array<std::uint32_t,3>{0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF}
            && &typed->TimerForAnalysis()!=&object.TimerForAnalysis(),"Dialog fresh constructor clone and timer");
        host.missingWindow=true;bool refused=false;
        try { object.FlowSlot24ForAnalysis(); }catch(const std::logic_error&) { refused=true; }
        Check(refused,"Missing borrowed Dialog window refuses");
        wxDebugMenuGameFlowState::SetFactoryHostForAnalysis(nullptr);
        wxDialogWindowGameFlowState::SetFactoryHostForAnalysis(&host);
        auto created=spRTTIManager::Instance().Create(wxDialogWindowGameFlowState::ClassID);
        wxDialogWindowGameFlowState::SetFactoryHostForAnalysis(nullptr);
        Check(created&&created->IsKindOf(wxGameFlowState::ClassID),"Dialog registered derived factory");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--emit") { Emit();return 0; }
        TestDebug();TestDialog();
        std::cout<<"PASS Debug/Dialog game-flow hooks, owner reload, window bounds and clone lifetime\n";return 0;
    }
    catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n';return 1; }
}
