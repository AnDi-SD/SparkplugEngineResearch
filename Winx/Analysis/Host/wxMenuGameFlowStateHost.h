#pragma once
#include "wxGameFlowStateHost.h"
#include "Analysis/Host/spNotificationHost.h"
#include "wxGameFlowNavigationHost.h"
#include <stdexcept>
#include <cstddef>

namespace sparkplug::reconstruction
{class spProjectionManager;class spProjectionManagerHost;class spNode;}

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
        // Storage-only raw allocation of at least the requested writable bytes.
        // Source owns publication and byte copy; older hook hosts can refuse.
        virtual void* AllocateMenuPathBufferForAnalysis(std::size_t)
        {throw std::logic_error("Menu Slot40 requires path storage");}
        virtual void* ResolveMenuGameContextForAnalysis()=0;
        virtual void* ReadMenuNotificationTarget2B4ForAnalysis(void* context)=0;
        virtual void* ResolveMenuGUIManagerForAnalysis()=0;
        virtual void SetMenuViewEnabledForAnalysis(void* manager,std::uint32_t view,std::uint32_t enabled)=0;
        // PC5593A0 consumes only view (RET4). The flag remains on the caller's
        // stack until PC55D1C0 consumes it (RET4); it is not a lookup argument.
        virtual void* FindMenuWindowPCForAnalysis(void* manager,std::uint32_t view)=0;
        virtual void ApplyMenuWindowPCForAnalysis(void* window,std::uint32_t flag)=0;
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
        // PC Entry/owned44 structural bindings. Source directly executes the
        // shared manager, GameLevel helper and Node update. Borrowed records
        // and graph objects must remain alive during the synchronous call.
        virtual void* ResolveMenuEngineCoreForAnalysis()
        {throw std::logic_error("Menu owned44 requires Core");}
        virtual void* ReadMenuScene18ForAnalysis(void*)
        {throw std::logic_error("Menu owned44 requires Core member18");}
        virtual sparkplug::reconstruction::spProjectionManager* ReadMenuProjectionManager2CForAnalysis(void*)
        {throw std::logic_error("Menu owned44 requires Scene projection manager2C");}
        virtual sparkplug::reconstruction::spProjectionManagerHost& MenuProjectionHostForAnalysis()
        {throw std::logic_error("Menu owned44 requires shared Projection bindings");}
        virtual sparkplug::reconstruction::spNode* ReadMenuSystemRoot14ForAnalysis(void*)
        {throw std::logic_error("Menu owned44 requires Scene SystemRoot14");}
        // PC Slot30 dependencies. Default refusal leaves older hook-only hosts
        // usable; no supplied method replaces Menu dispatch or shared navigation.
        virtual std::uint8_t ReadMenuInputByte1BForAnalysis(void*)
        {throw std::logic_error("Menu Slot30 requires borrowed input byte1B");}
        virtual void* ReadMenuActivatedWidget134ForAnalysis(void*)
        {throw std::logic_error("Menu Slot30 requires window member134");}
        // Stable borrowed NUL-terminated buffer. Its contents may change during
        // the foreign flow command; Menu compares it after that callback.
        virtual const char* ReadMenuWidgetNameForAnalysis(void*)
        {throw std::logic_error("Menu Slot30 requires a borrowed widget name");}
        virtual void* ResolveMenuFlowManagerForAnalysis()
        {throw std::logic_error("Menu Slot30 requires the flow manager");}
        virtual void InvokeMenuFlow5954C0ForAnalysis(void*,std::uint32_t,std::uint32_t)
        {throw std::logic_error("Menu foreign flow5954C0 is unbound");}
        virtual void InvokeMenuFlow595450ForAnalysis(void*,std::uint32_t,std::uint32_t)
        {throw std::logic_error("Menu foreign flow595450 is unbound");}
        virtual wxGameFlowNavigationHost& MenuNavigationHostForAnalysis()
        {throw std::logic_error("Menu Slot30 requires shared navigation bindings");}
    };
}
