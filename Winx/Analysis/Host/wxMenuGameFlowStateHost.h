#pragma once
#include "wxGameFlowStateHost.h"
#include "Analysis/Host/spNotificationHost.h"

namespace winx::reconstruction
{
    class wxMenuGameFlowState;
    class wxMenuGameFlowStateHost : public wxGameFlowStateHost,public sparkplug::reconstruction::spNotificationHost
    {
    public:
        enum class HookProfileForAnalysis{PC,PS2}; // own hooks only; parent ctor is PC
        virtual HookProfileForAnalysis MenuHookProfileForAnalysis() const noexcept=0;
        virtual void RegisterMenuReceiverForAnalysis(wxMenuGameFlowState&,std::uint32_t tag)=0;
        virtual void UnregisterMenuReceiverForAnalysis(wxMenuGameFlowState&,std::uint32_t tag)=0;
        // Storage-only ownership transfers. The source constructs and destroys
        // real shared timer/object bodies. Take must return the exact requested
        // pointer, never a successful replacement for native owned logic.
        virtual void StoreMenuTimerAllocationForAnalysis(std::unique_ptr<sparkplug::reconstruction::spTaskTimer>)=0;
        virtual std::unique_ptr<sparkplug::reconstruction::spTaskTimer> TakeMenuTimerAllocationForAnalysis(
            sparkplug::reconstruction::spTaskTimer&)=0;
        virtual std::unique_ptr<sparkplug::reconstruction::spBaseObject> TakeMenuObjectAllocationForAnalysis(
            sparkplug::reconstruction::spBaseObject&)=0;
        virtual void FreeMenuAllocationForAnalysis(void* allocation)=0;
        virtual void* ResolveMenuGameContextForAnalysis()=0;
        virtual void* ReadMenuNotificationTarget2B4ForAnalysis(void* context)=0;
        virtual void* ResolveMenuGUIManagerForAnalysis()=0;
        virtual void SetMenuViewEnabledForAnalysis(void* manager,std::uint32_t view,std::uint32_t enabled)=0;
        virtual void* FindMenuWindowPCForAnalysis(void* manager,std::uint32_t view,std::uint32_t flag)=0;
        virtual void ApplyMenuWindowPCForAnalysis(void* window)=0;
        virtual void* FindMenuWindowPS2ForAnalysis(void* manager,std::uint32_t view)=0;
        virtual void ApplyMenuWindowPS2ForAnalysis(void* window,std::uint32_t flag)=0;
        virtual void SetMenuHUDForAnalysis(void* manager,std::uint32_t value)=0;
        virtual void* ResolveMenuAudioInterfaceForAnalysis()=0;
        virtual void* GetMenuAudioControllerForAnalysis(void* audio)=0;
        virtual void SetMenuAudioEnabledForAnalysis(void* controller,std::uint32_t enabled)=0;
        virtual void* ResolveMenuExitServiceForAnalysis()=0;
        virtual void InvokeMenuExitServiceForAnalysis(void* service,std::uint32_t value)=0;
        virtual void* ResolveMenuInputOwnerForAnalysis()=0;
        virtual void* ReadMenuInput1CForAnalysis(void* owner)=0;
        virtual void* ReadMenuInput18ForAnalysis(void* owner)=0;
        virtual std::uint8_t ReadMenuInputByte18ForAnalysis(void* input)=0;
        virtual void ResetMenuInputForAnalysis(void* input)=0;
        virtual void SetMenuInputModeForAnalysis(void* input,std::uint32_t rawMode)=0;
        virtual std::uint8_t& BorrowMenuSharedFlag45ForAnalysis()=0;
    };
}
