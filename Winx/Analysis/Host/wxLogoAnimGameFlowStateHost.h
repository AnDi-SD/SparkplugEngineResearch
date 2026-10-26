#pragma once
#include "wxGameFlowStateHost.h"
#include "wxLogoResourceLoadingHost.h"

namespace winx::reconstruction
{
    class wxLogoAnimGameFlowState;
    class wxLogoAnimGameFlowStateHost : public wxGameFlowStateHost,public wxLogoResourceLoadingHost
    {
    public:
        enum class HookProfileForAnalysis{PC,PS2}; // own hooks only; parent constructor is PC
        virtual HookProfileForAnalysis LogoHookProfileForAnalysis() const noexcept=0;
        virtual void RegisterLogoReceiverForAnalysis(wxLogoAnimGameFlowState&,std::uint32_t tag)=0;
        virtual void UnregisterLogoReceiverForAnalysis(wxLogoAnimGameFlowState&,std::uint32_t tag)=0;
        virtual void* ResolveLogoServiceForAnalysis()=0;
        // PC counterparts48EAA0/5B7A00 are literal no-ops. PS2 service bodies
        // operate on the independently resolved global receiver, not Logo.
        virtual void InvokePS2LogoServiceEntryForAnalysis(void* service)=0;
        virtual void InvokePS2LogoServiceExitForAnalysis(void* service,std::string_view tag)=0;
        virtual void* ResolveLogoAudioInterfaceForAnalysis()=0;
        virtual void* GetLogoAudioControllerForAnalysis(void* audio)=0;
        virtual void SetLogoAudioEnabledForAnalysis(void* controller,std::uint32_t value)=0;
        virtual void* ResolveHookInputOwnerForAnalysis()=0;
        virtual void* ReadHookInputForAnalysis(void* owner)=0; // member1C, not cached18
        virtual void ResetHookInputForAnalysis(void* input)=0;
        virtual void SetHookInputModeForAnalysis(void* input,std::uint32_t mode)=0;
        virtual std::uint32_t ReadLogoTimerClockForAnalysis()=0; // distinct sample each call
        virtual void* ResolveLogoAppOwnerForAnalysis()=0;
        virtual std::uint8_t ReadLogoAppByte4BForAnalysis(void* owner)=0;
        virtual std::uint32_t ReadLogoAppWord60ForAnalysis(void* owner)=0;
        // Four separate native globals at stride0C, represented portably.
        virtual std::array<std::uint32_t,4>& BorrowLogoGlobalWordsForAnalysis()=0;
    };
}
