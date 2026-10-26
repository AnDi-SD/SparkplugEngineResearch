#pragma once

#include "wxGameFlowStateHost.h"
#include <cstdint>

namespace winx::reconstruction
{
    // Required borrowed input/debug services. Input member1C is independent
    // from the parent constructor's cached member18. No default success body.
    class wxDebugMenuGameFlowStateHost : public wxGameFlowStateHost
    {
    public:
        virtual void* ResolveHookInputOwnerForAnalysis() = 0;
        virtual void* ReadHookInputForAnalysis(void* owner) = 0;
        virtual void ResetHookInputForAnalysis(void* input) = 0;
        virtual void SetHookInputModeForAnalysis(void* input, std::uint32_t mode) = 0;
        virtual void* ResolveDebugServiceForAnalysis() = 0;
        virtual void InvokeDebugSlot20ConsumerForAnalysis(void* service) = 0; // PC5CCEC0
        virtual void InvokeDebugSlot24ConsumerForAnalysis(void* service) = 0; // PC5CDB90
    };
}
