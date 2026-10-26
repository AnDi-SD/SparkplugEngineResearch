#include "wxMenuGameFlowState.h"
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
            auto* window=Require(menuHost_.FindMenuWindowPCForAnalysis(gui,view,flag));
            menuHost_.ApplyMenuWindowPCForAnalysis(window);
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
    void wxMenuGameFlowState::FlowSlot20ForAnalysis(){throw std::logic_error("Menu Entry depends on unrecovered owned44 collection/retry");}
    bool wxMenuGameFlowState::MenuSlot44ForAnalysis(){throw std::logic_error("Menu owned44 collection/retry is outside the recovered contract");}
    bool wxMenuGameFlowState::FlowSlot30ForAnalysis(){throw std::logic_error("Menu Slot30 input/navigation dispatch is outside the recovered contract");}
}
