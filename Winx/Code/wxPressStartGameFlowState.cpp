#include "wxPressStartGameFlowState.h"
#include "Code/SparkBase/spNotification.h"
#include "Code/SparkBase/spBroadcastNotification.h"
#include "Code/SparkBase/spSubscriptionManager.h"
#include "Code/Sparkplug/spGameLevel.h"
#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxPressStartGameFlowStateHost* factoryHost=nullptr;
        std::unique_ptr<spBaseObject> CreateState()
        {
            if(!factoryHost)throw std::logic_error("PressStart requires a factory host");
            return std::make_unique<wxPressStartGameFlowState>(*factoryHost);
        }
        const spRTTIRecord record{wxPressStartGameFlowState::ClassID,wxMenuGameFlowState::ClassID,
            "wxPressStartGameFlowState",&wxMenuGameFlowState::StaticRTTI(),&CreateState,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        void* Require(void* value)
        {
            if(!value)throw std::logic_error("PressStart requires a live borrowed input/context/flow binding");
            return value; // Analytical refusal, not native lazy creation/fault behavior.
        }
    }
    wxPressStartGameFlowState::wxPressStartGameFlowState(wxPressStartGameFlowStateHost& host)
        :wxMenuGameFlowState(host),pressStartHost_(host),embeddedTimer_(0,{})
    {
        // The actual shared spTimer is already constructed inactive. Reload
        // the external member18 after both parent constructors and this timer.
        auto& menu=MenuStateForFixture();menu.view40=8;menu.mode44=0;
        pressStart_.borrowedInput5C=pressStartHost_.BorrowInputForAnalysis();
    }
    wxPressStartGameFlowState::~wxPressStartGameFlowState()=default;
    void wxPressStartGameFlowState::SetFactoryHostForAnalysis(wxPressStartGameFlowStateHost* host) noexcept
    {factoryHost=host;}
    const spRTTIRecord& wxPressStartGameFlowState::StaticRTTI() noexcept {(void)registered;return record;}
    const spRTTIRecord& wxPressStartGameFlowState::vfunc_18() const noexcept {return record;}
    std::unique_ptr<spBaseObject> wxPressStartGameFlowState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxPressStartGameFlowState>(pressStartHost_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    void wxPressStartGameFlowState::FlowSlot20ForAnalysis()
    {
        if(pressStartHost_.PressStartEntryProfileForAnalysis()
                !=wxPressStartGameFlowStateHost::EntryProfileForAnalysis::PCEmptyOwnedGameLevel
            ||pressStartHost_.MenuHookProfileForAnalysis()
                !=wxMenuGameFlowStateHost::HookProfileForAnalysis::PC)
            throw std::logic_error("PressStart Entry profile is unqualified");
        auto* level=dynamic_cast<spGameLevel*>(GetMenuStateForAnalysis().object4C);
        if(!level||level->GetName()!=nullptr||level->GetInstanceCountForAnalysis()!=0)
            throw std::logic_error("PressStart Entry requires an admitted empty owned PC GameLevel profile");
        // Admission does not replace any parent work. The caller supplies the
        // stable existing real empty ProjectionManager/plain Node bindings.
        wxMenuGameFlowState::FlowSlot20ForAnalysis();
        auto* gui=Require(pressStartHost_.ResolveMenuGUIManagerForAnalysis());
        const auto view=GetMenuStateForAnalysis().view40;
        auto* window=Require(pressStartHost_.FindMenuWindowPCForAnalysis(gui,view));
        pressStartHost_.InvokePressStartWindowConsumer55D570ForAnalysis(window,0x707F94);
        pressStartHost_.BorrowMenuSharedFlag45ForAnalysis()=0;
        gui=Require(pressStartHost_.ResolveMenuGUIManagerForAnalysis());
        pressStartHost_.SetMenuHUDForAnalysis(gui,0);
        auto* inputManager=Require(pressStartHost_.ResolvePressStartDXInputManagerForAnalysis());
        auto* device=Require(pressStartHost_.InvokePressStartDXInputManagerView18Slot0CForAnalysis(inputManager));
        pressStartHost_.SetPressStartDXInputDeviceSlot20ForAnalysis(device,0);
        auto* owner=Require(pressStartHost_.ResolveMenuInputOwnerForAnalysis());
        auto* input=Require(pressStartHost_.ReadMenuInput1CForAnalysis(owner));
        const auto rawMode=GetMenuStateForAnalysis().mode44;
        pressStartHost_.ResetMenuInputForAnalysis(input);
        std::int32_t signedMode;std::memcpy(&signedMode,&rawMode,sizeof(signedMode));
        if(signedMode<10)
            pressStartHost_.SetMenuInputModeForAnalysis(
                Require(pressStartHost_.ReadMenuInput1CForAnalysis(owner)),rawMode);
        // Reset/SetMode may replace the global owner; this read is fresh.
        owner=Require(pressStartHost_.ResolveMenuInputOwnerForAnalysis());
        pressStartHost_.BorrowPressStartInputOwnerFlag20ForAnalysis(owner)=1;
        pressStart_.capturedWord64=0;
        const spTimer::ClockSourceForAnalysis clock=[this]{return pressStartHost_.ReadPressStartTimerClockForAnalysis();};
        if(!embeddedTimer_.ResetForAnalysis(clock)||!embeddedTimer_.StartForAnalysis(clock))
            throw std::logic_error("PressStart Entry requires a Timer clock binding");
    }
    void wxPressStartGameFlowState::FlowSlot24ForAnalysis()
    {
        if(pressStartHost_.PressStartExitProfileForAnalysis()
                !=wxPressStartGameFlowStateHost::ExitProfileForAnalysis::PCStableMenuAndSubscriptions
            ||pressStartHost_.MenuHookProfileForAnalysis()
                !=wxMenuGameFlowStateHost::HookProfileForAnalysis::PC
            ||!spSubscriptionManager::GetInstance())
            throw std::logic_error("PressStart Exit requires an admitted PC profile and existing shared subscriptions");
        if(auto* owned=GetMenuStateForAnalysis().object4C)
        {
            auto* level=dynamic_cast<spGameLevel*>(owned);
            if(!level||level->GetName()!=nullptr||level->GetInstanceCountForAnalysis()!=0)
                throw std::logic_error("PressStart Exit owned lifetime profile requires an empty null-name GameLevel");
        }
        // Native Menu builds its directed frame inline; use the existing real
        // parent sequence. Its foreign callbacks may replace global bindings.
        wxMenuGameFlowState::FlowSlot24ForAnalysis();
        auto* owner=Require(pressStartHost_.ResolveMenuInputOwnerForAnalysis());
        pressStartHost_.ResetMenuInputForAnalysis(Require(pressStartHost_.ReadMenuInput1CForAnalysis(owner)));
        // PC5EFF1C writes the whole scratch DWORD to0, not merely its low byte.
        // The shared builder records key11 in word0C and payload9C in word18.
        if(SendBroadcastNotificationForAnalysis(this,0x27D1,0x11,0x9C,0)
            !=spBroadcastNotificationResultForAnalysis::Completed)
            throw std::logic_error("PressStart Exit requires the existing shared SubscriptionManager");
        // No trailing own stores or Timer operation: callback changes survive.
    }
    bool wxPressStartGameFlowState::FlowSlot30ForAnalysis()
    {
        if(pressStart_.field60 && pressStartHost_.ReadPressStartCachedInputByte1BForAnalysis(
            Require(pressStart_.borrowedInput5C)))
        {
            pressStart_.field60=0;
            auto* context=Require(pressStartHost_.ResolveMenuGameContextForAnalysis());
            auto* target=pressStartHost_.ReadMenuNotificationTarget2B4ForAnalysis(context);
            SendDirectedNotificationForAnalysis(pressStartHost_,this,target,0x27D1,0x11,0);
            // Resolve after dispatch: that callback may replace the global
            // manager or own state. No trailing clear overwrites its stores.
            auto* flow=Require(pressStartHost_.ResolveMenuFlowManagerForAnalysis());
            pressStartHost_.InvokeMenuFlow595450ForAnalysis(flow,0x38,0);
            return true;
        }
        // Preserve the native second load, including a fresh cached pointer.
        if(!pressStartHost_.ReadPressStartCachedInputByte1BForAnalysis(Require(pressStart_.borrowedInput5C)))
            pressStart_.field60=1;
        return true;
    }
}
