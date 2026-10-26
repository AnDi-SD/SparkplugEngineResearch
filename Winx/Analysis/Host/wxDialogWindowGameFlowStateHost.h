#pragma once

#include "wxGameFlowStateHost.h"
#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxDialogWindowGameFlowStateHost : public wxGameFlowStateHost
    {
    public:
        // Exactly the borrowed window range[14,26C), not its complete layout.
        using ResetRegionForAnalysis = std::array<std::uint8_t,0x258>;
        virtual void* ResolveDialogWindowForAnalysis() = 0;
        virtual ResetRegionForAnalysis& BorrowDialogWindowResetRegionForAnalysis(void* window) = 0;
        virtual void InvokeDialogWindowConsumerForAnalysis(void* window) = 0; // PC5C98B0 / PS2 36C400
    };
}
