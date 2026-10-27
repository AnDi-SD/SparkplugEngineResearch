#include "wxMenuGameFlowState.h"
#include "wxGameFlowNavigation.h"
#include "Code/Sparkplug/spProjectionManager.h"
#include "Code/Sparkplug/spGameLevel.h"
#include "Code/Sparkplug/spGameLevelInitialization.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/SparkBase/spNotification.h"
#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxMenuGameFlowStateHost* factoryHost=nullptr;
        std::unique_ptr<spBaseObject> CreateState()
        {
            if(!factoryHost)throw std::logic_error("wxMenuGameFlowState requires a factory host");
            return std::make_unique<wxMenuGameFlowState>(*factoryHost);
        }
        const spRTTIRecord record{wxMenuGameFlowState::ClassID,wxGameFlowState::ClassID,
            "wxMenuGameFlowState",&wxGameFlowState::StaticRTTI(),&CreateState,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        template<class T>T* Require(T* value)
        {if(!value)throw std::logic_error("Menu requires a live borrowed provider/timer");return value;}
    }
    wxMenuGameFlowState::wxMenuGameFlowState(wxMenuGameFlowStateHost& host)
        :wxGameFlowState(host),menuHost_(host)
    {
        menuHost_.RegisterMenuReceiverForAnalysis(*this,0x20);
        menu_.borrowedInput54=menuHost_.BorrowInputForAnalysis();
        auto timer=std::make_unique<spTaskTimer>();auto* created=timer.get();
        menuHost_.StoreMenuTimerAllocationForAnalysis(std::move(timer));menu_.timer50=created;
        created->ResetForAnalysis();
        // Native +28 is Reset, +20 is Start (not Pause). Reload the field
        // between the two virtual calls; the clock may subsequently replace it.
        if(!Require(menu_.timer50)->StartForAnalysis([this]{return menuHost_.ReadClockForAnalysis();}))
            throw std::logic_error("Menu requires a nonzero clock divisor");
    }
    wxMenuGameFlowState::~wxMenuGameFlowState()
    {
        if(auto* value=menu_.object4C)
        {
            auto object=menuHost_.TakeMenuObjectAllocationForAnalysis(*value);
            if(object.get()!=value)std::terminate(); // invalid storage provider, not native return
            object.reset();menu_.object4C=nullptr;
        }
        if(auto* value=menu_.allocation3C)
        {menuHost_.FreeMenuAllocationForAnalysis(value);menu_.allocation3C=nullptr;}
        if(auto* value=menu_.timer50)
        {
            auto timer=menuHost_.TakeMenuTimerAllocationForAnalysis(*value);
            if(timer.get()!=value)std::terminate();
            timer.reset();menu_.timer50=nullptr;
        }
        menuHost_.UnregisterMenuReceiverForAnalysis(*this,0x20);
    }
    void wxMenuGameFlowState::SetFactoryHostForAnalysis(wxMenuGameFlowStateHost* host) noexcept{factoryHost=host;}
    const spRTTIRecord& wxMenuGameFlowState::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& wxMenuGameFlowState::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> wxMenuGameFlowState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxMenuGameFlowState>(menuHost_);manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    void wxMenuGameFlowState::FlowSlot24ForAnalysis()
    {
        auto* context=Require(menuHost_.ResolveMenuGameContextForAnalysis());
        auto* target=menuHost_.ReadMenuNotificationTarget2B4ForAnalysis(context);
        SendDirectedNotificationForAnalysis(menuHost_,this,target,0x2764,0,0);
        auto* gui=Require(menuHost_.ResolveMenuGUIManagerForAnalysis());
        const auto view=menu_.view40;menuHost_.SetMenuViewEnabledForAnalysis(gui,view,0);
        MutableStateForAnalysis().field25=0;
        auto* audio=Require(menuHost_.ResolveMenuAudioInterfaceForAnalysis());
        auto* controller=Require(menuHost_.GetMenuAudioControllerForAnalysis(audio));
        menuHost_.SetMenuAudioEnabledForAnalysis(controller,0);
        gui=Require(menuHost_.ResolveMenuGUIManagerForAnalysis());menuHost_.SetMenuHUDForAnalysis(gui,0);
        auto* service=Require(menuHost_.ResolveMenuExitServiceForAnalysis());menuHost_.InvokeMenuExitServiceForAnalysis(service,1);
        auto* owner=Require(menuHost_.ResolveMenuInputOwnerForAnalysis());
        menuHost_.ResetMenuInputForAnalysis(Require(menuHost_.ReadMenuInput1CForAnalysis(owner)));
    }
    void wxMenuGameFlowState::ApplyWindowForAnalysis(std::uint32_t flag)
    {
        auto* gui=Require(menuHost_.ResolveMenuGUIManagerForAnalysis());const auto view=menu_.view40;
        if(menuHost_.MenuHookProfileForAnalysis()==wxMenuGameFlowStateHost::HookProfileForAnalysis::PC)
        {
            auto* window=Require(menuHost_.FindMenuWindowPCForAnalysis(gui,view));
            menuHost_.ApplyMenuWindowPCForAnalysis(window,flag);
        }
        else
        {
            auto* window=Require(menuHost_.FindMenuWindowPS2ForAnalysis(gui,view));
            menuHost_.ApplyMenuWindowPS2ForAnalysis(window,flag);
        }
    }
    void wxMenuGameFlowState::FlowSlot28ForAnalysis()
    {
        ApplyWindowForAnalysis(0);auto* gui=Require(menuHost_.ResolveMenuGUIManagerForAnalysis());
        menuHost_.SetMenuHUDForAnalysis(gui,0);
    }
    void wxMenuGameFlowState::FlowSlot2CForAnalysis()
    {
        void* owner;void* input;std::uint32_t rawMode;
        if(menuHost_.MenuHookProfileForAnalysis()==wxMenuGameFlowStateHost::HookProfileForAnalysis::PC)
        {
            owner=Require(menuHost_.ResolveMenuInputOwnerForAnalysis());input=Require(menuHost_.ReadMenuInput1CForAnalysis(owner));rawMode=menu_.mode44;
        }
        else
        {
            rawMode=menu_.mode44;owner=Require(menuHost_.ResolveMenuInputOwnerForAnalysis());input=Require(menuHost_.ReadMenuInput1CForAnalysis(owner));
        }
        menuHost_.ResetMenuInputForAnalysis(input);
        std::int32_t signedMode;std::memcpy(&signedMode,&rawMode,sizeof(signedMode));
        if(signedMode<10)
            menuHost_.SetMenuInputModeForAnalysis(Require(menuHost_.ReadMenuInput1CForAnalysis(owner)),rawMode);
        ApplyWindowForAnalysis(1);
        owner=Require(menuHost_.ResolveMenuInputOwnerForAnalysis());
        input=Require(menuHost_.ReadMenuInput18ForAnalysis(owner));const auto flag=menuHost_.ReadMenuInputByte18ForAnalysis(input);
        if(flag)
        {
            menuHost_.BorrowMenuSharedFlag45ForAnalysis()=0;
            auto* gui=Require(menuHost_.ResolveMenuGUIManagerForAnalysis());menuHost_.SetMenuHUDForAnalysis(gui,0);
            auto* audio=Require(menuHost_.ResolveMenuAudioInterfaceForAnalysis());auto* controller=Require(menuHost_.GetMenuAudioControllerForAnalysis(audio));
            menuHost_.SetMenuAudioEnabledForAnalysis(controller,0);
        }
        else
        {
            menuHost_.BorrowMenuSharedFlag45ForAnalysis()=1;
            auto* audio=Require(menuHost_.ResolveMenuAudioInterfaceForAnalysis());auto* controller=Require(menuHost_.GetMenuAudioControllerForAnalysis(audio));
            menuHost_.SetMenuAudioEnabledForAnalysis(controller,1);
            auto* gui=Require(menuHost_.ResolveMenuGUIManagerForAnalysis());menuHost_.SetMenuHUDForAnalysis(gui,1);
        }
        MutableStateForAnalysis().field25=0;
    }
    void wxMenuGameFlowState::FlowSlot20ForAnalysis()
    {
        if(menuHost_.MenuHookProfileForAnalysis()!=wxMenuGameFlowStateHost::HookProfileForAnalysis::PC)
            throw std::logic_error("Menu Entry is qualified only for PC");
        (void)MenuSlot44ForAnalysis();
        auto* gui=Require(menuHost_.ResolveMenuGUIManagerForAnalysis());
        const auto view=menu_.view40;menuHost_.SetMenuViewEnabledForAnalysis(gui,view,1);
        auto* owner=Require(menuHost_.ResolveMenuInputOwnerForAnalysis());
        auto* input=Require(menuHost_.ReadMenuInput18ForAnalysis(owner));
        const auto flag=menuHost_.ReadMenuInputByte18ForAnalysis(input);
        if(flag)
        {
            menuHost_.BorrowMenuSharedFlag45ForAnalysis()=0;
            gui=Require(menuHost_.ResolveMenuGUIManagerForAnalysis());menuHost_.SetMenuHUDForAnalysis(gui,0);
            auto* audio=Require(menuHost_.ResolveMenuAudioInterfaceForAnalysis());
            auto* controller=Require(menuHost_.GetMenuAudioControllerForAnalysis(audio));
            menuHost_.SetMenuAudioEnabledForAnalysis(controller,0);
        }
        else
        {
            menuHost_.BorrowMenuSharedFlag45ForAnalysis()=1;
            auto* audio=Require(menuHost_.ResolveMenuAudioInterfaceForAnalysis());
            auto* controller=Require(menuHost_.GetMenuAudioControllerForAnalysis(audio));
            menuHost_.SetMenuAudioEnabledForAnalysis(controller,1);
            gui=Require(menuHost_.ResolveMenuGUIManagerForAnalysis());menuHost_.SetMenuHUDForAnalysis(gui,1);
        }
        // Entry has no trailing field25 clear. Foreign UI/audio effects survive.
    }
    bool wxMenuGameFlowState::MenuSlot44ForAnalysis()
    {
        if(menuHost_.MenuHookProfileForAnalysis()!=wxMenuGameFlowStateHost::HookProfileForAnalysis::PC)
            throw std::logic_error("Menu owned44 is qualified only for PC");
        auto* core=Require(menuHost_.ResolveMenuEngineCoreForAnalysis());
        auto* scene=Require(menuHost_.ReadMenuScene18ForAnalysis(core));
        auto* projection=Require(menuHost_.ReadMenuProjectionManager2CForAnalysis(scene));
        (void)projection->InitializeProjectionsForAnalysis(menuHost_.MenuProjectionHostForAnalysis());
        // Reload own4C after projection callbacks. The original factory and
        // typed callers establish a GameLevel, not a timer or opaque helper.
        auto* level=dynamic_cast<spGameLevel*>(menu_.object4C);
        if(!level)throw std::logic_error("Menu owned44 requires an established GameLevel");
        const auto result=InitializeGameLevelForAnalysis(*level);
        if(result.status!=spGameLevelInitializationStatusForAnalysis::Completed)
            throw std::logic_error("Menu owned44 GameLevel initialization refused its remaining prefix");
        // GameLevel notifications can replace Core/Scene. Read them again.
        core=Require(menuHost_.ResolveMenuEngineCoreForAnalysis());
        scene=Require(menuHost_.ReadMenuScene18ForAnalysis(core));
        auto* root=Require(menuHost_.ReadMenuSystemRoot14ForAnalysis(scene));
        (void)root->UpdateWorldForAnalysis(0);
        return true;
    }
    bool wxMenuGameFlowState::MenuSlot38ForAnalysis()
    {
        if(menuHost_.MenuHookProfileForAnalysis()!=wxMenuGameFlowStateHost::HookProfileForAnalysis::PC)
            throw std::logic_error("Menu Slot38 is qualified only for PC");
        return true;
    }
    void wxMenuGameFlowState::MenuSlot40ForAnalysis(const char* path)
    {
        if(menuHost_.MenuHookProfileForAnalysis()!=wxMenuGameFlowStateHost::HookProfileForAnalysis::PC)
            throw std::logic_error("Menu Slot40 is qualified only for PC");
        if(auto* previous=menu_.allocation3C)
        {
            menuHost_.FreeMenuAllocationForAnalysis(previous);
            menu_.allocation3C=nullptr;
        }
        if(!path)return;
        // Input remains externally alive. Free callbacks can change its bytes
        // before this length read; allocation callbacks can change them again.
        const auto bytes=std::strlen(path)+1;
        auto* allocation=menuHost_.AllocateMenuPathBufferForAnalysis(bytes);
        menu_.allocation3C=allocation;
        if(!allocation)throw std::logic_error("Menu path allocation is unavailable");
        auto* destination=static_cast<char*>(allocation);
        for(std::size_t i=0;i<bytes;++i)
        {
            const auto value=path[i];destination[i]=value;
            if(value==0)return;
        }
        // Native strcpy would continue beyond the requested storage. Retain
        // completed prefix effects and refuse that unqualified unsafe input.
        throw std::logic_error("Menu path grew beyond its requested allocation");
    }
    bool wxMenuGameFlowState::FlowSlot30ForAnalysis()
    {
        if(menuHost_.MenuHookProfileForAnalysis()!=wxMenuGameFlowStateHost::HookProfileForAnalysis::PC)
            throw std::logic_error("Menu Slot30 is qualified only for PC");
        // Native captures50 before virtual Update. A null extra timer skips
        // only this update; the shared navigation still updates parent2C.
        auto* timer=menu_.timer50;
        if(timer&&!timer->UpdateForAnalysis([this]{return menuHost_.ReadClockForAnalysis();}))
            throw std::logic_error("Menu Slot30 clock is invalid");
        auto& state=MutableStateForAnalysis();
        if(state.field25&&menuHost_.ReadMenuInputByte1BForAnalysis(Require(menu_.borrowedInput54)))
        {
            state.field25=0;
            auto* gui=Require(menuHost_.ResolveMenuGUIManagerForAnalysis());
            const auto view=menu_.view40;
            auto* window=Require(menuHost_.FindMenuWindowPCForAnalysis(gui,view));
            if(auto* widget=menuHost_.ReadMenuActivatedWidget134ForAnalysis(window))
            {
                const auto* name=Require(menuHost_.ReadMenuWidgetNameForAnalysis(widget));
                auto* flow=Require(menuHost_.ResolveMenuFlowManagerForAnalysis());
                menuHost_.InvokeMenuFlow5954C0ForAnalysis(flow,53,0);
                struct Choice final{const char* name;std::uint32_t code;};
                static constexpr Choice choices[]{
                    {"LoadGardenia_01",1},{"LoadGardenia_02",2},{"LoadGardenia_03",3},{"LoadGardenia_04",50},
                    {"LoadDomino_01",4},{"LoadDomino_02",5},{"LoadDomino_03",6},{"LoadDomino_04",7},{"LoadDomino_05",8},
                    {"LoadSwamp_01",9},{"LoadSwamp_02",10},{"LoadSwamp_03",11}};
                for(const auto& choice:choices)
                    if(std::strcmp(name,choice.name)==0)
                    {
                        flow=Require(menuHost_.ResolveMenuFlowManagerForAnalysis());
                        menuHost_.InvokeMenuFlow595450ForAnalysis(flow,choice.code,0);
                        break;
                    }
            }
        }
        else if(!menuHost_.ReadMenuInputByte1BForAnalysis(Require(menu_.borrowedInput54)))
            state.field25=1;
        // Capture the latest view after foreign commands, then execute the
        // common recovered navigation body. Never replace it with a success seam.
        const auto view=menu_.view40;
        auto& navigation=menuHost_.MenuNavigationHostForAnalysis();
        NavigateGameFlowPCForAnalysis(*this,view,navigation,[this]{return menuHost_.ReadClockForAnalysis();});
        return true;
    }
}
