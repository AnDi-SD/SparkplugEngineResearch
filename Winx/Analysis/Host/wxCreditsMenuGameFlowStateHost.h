#pragma once

#include "wxGameFlowStateHost.h"
#include <cstdint>

namespace winx::reconstruction
{
    class wxCreditsMenuGameFlowState;

    // All services are borrowed and must outlive the synchronous operation.
    // Read/borrow accessors expose literal fields and do not simulate foreign
    // methods. No callback implements the Credits completion/flag decisions.
    class wxCreditsMenuGameFlowStateHost : public wxGameFlowStateHost
    {
    public:
        virtual void RegisterCreditsReceiverForAnalysis(wxCreditsMenuGameFlowState&,std::uint32_t tag) = 0;
        virtual void UnregisterCreditsReceiverForAnalysis(wxCreditsMenuGameFlowState&,std::uint32_t tag) = 0;

        virtual void* ResolveCreditsAudioInterfaceForAnalysis() = 0; // manager's embedded+18 view
        virtual void* GetCreditsAudioControllerForAnalysis(void* audioInterface) = 0;
        virtual void SetCreditsAudioEnabledForAnalysis(void* controller,std::uint32_t enabled) = 0;
        virtual std::uint8_t& BorrowCreditsSharedFlag45ForAnalysis() = 0;
        virtual void* ResolveCreditsHUDForAnalysis() = 0;
        virtual void InvokeCreditsHUDForAnalysis(void* hud,std::uint32_t value) = 0;

        // Hook input member1C and cached input member18 are distinct. The
        // inherited BorrowInputForAnalysis is called again by Credits ctor.
        virtual void* ResolveHookInputOwnerForAnalysis() = 0;
        virtual void* ReadHookInputForAnalysis(void* owner) = 0;
        virtual void ResetHookInputForAnalysis(void* input) = 0;
        virtual void SetHookInputModeForAnalysis(void* input,std::uint32_t mode) = 0;
        virtual std::uint8_t ReadCreditsCachedInput2AForAnalysis(void* cachedInput) = 0;
        virtual std::uint8_t& BorrowCreditsInputOwnerFlag20ForAnalysis(void* owner) = 0;

        virtual void* ResolveCreditsControllerForAnalysis() = 0;
        virtual std::uint32_t ReadCreditsControllerState1B0ForAnalysis(void* controller) = 0;
        virtual void RequestCreditsControllerStateForAnalysis(void* controller,std::uint32_t state,std::uint32_t parameter) = 0;
        virtual void SetCreditsControllerStateForAnalysis(void* controller,std::uint32_t state,std::uint32_t parameter) = 0;
        virtual void* ResolveCreditsServiceForAnalysis() = 0;
        virtual void SendCreditsServiceEventForAnalysis(void* service,std::uint32_t event,std::uint32_t parameter) = 0;
        virtual void* ResolveCreditsOnceServiceForAnalysis() = 0;
        virtual void SendCreditsOnceRequestForAnalysis(void* service,std::uint32_t value) = 0;
    };
}
