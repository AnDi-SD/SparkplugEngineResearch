#pragma once

#include "wxGameFlowStateHost.h"
#include <cstdint>

namespace winx::reconstruction
{
    // Borrowed services used by the derived hooks. These callbacks describe
    // operations owned by input/load-save/controller classes, not substitutes
    // for wxLoadSaveGameFlowState's recovered ordering and branch decisions.
    class wxLoadSaveGameFlowStateHost : public wxGameFlowStateHost
    {
    public:
        // Resolve the singleton owner, then read member1C. This differs from
        // the parent constructor's cached input at singleton member18.
        virtual void* ResolveHookInputOwnerForAnalysis() = 0;
        virtual void* ReadHookInputForAnalysis(void* owner) = 0;
        virtual void ResetHookInputForAnalysis(void* input) = 0;
        virtual void SetHookInputModeForAnalysis(void* input, std::uint32_t mode) = 0;

        virtual void* ResolveLoadSaveManagerForAnalysis() = 0;
        virtual std::uint32_t ReadLoadSaveStateForAnalysis(void* manager) = 0; // native +6A4
        virtual void* ResolveLoadSaveControllerForAnalysis() = 0;
        virtual void InvokeLoadSaveControllerForAnalysis(void* controller,
            std::uint32_t parameter, std::uint32_t value) = 0; // 36,1
        virtual void UpdateLoadSaveManagerForAnalysis(void* manager) = 0;
    };
}
