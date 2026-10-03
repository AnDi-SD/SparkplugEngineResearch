#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    enum class wxVulnerableStateProfileForAnalysis { PC, PS2 };

    // Our borrowed field/service adapter. The state owns every branch below;
    // none of these services substitutes an original state method.
    class wxVulnerableStateHost : public wxCharacterStateHost
    {
    public:
        virtual wxVulnerableStateProfileForAnalysis ProfileForAnalysis() const noexcept = 0;
        virtual const char* EventTagNameForAnalysis(const void* event) = 0;
        // PC4135E0 / PS2115B20, format "Animation tag : %s".
        virtual void LogAnimationTagForAnalysis(const char* tag) = 0;
        // PC owner124->14C / PS2 owner130->158.
        virtual std::uint32_t ReadOwnerEventKindForAnalysis(void* owner) = 0;
        // PC owner224 / PS2 owner230.
        virtual void WriteOwnerKneeFlagForAnalysis(void* owner, std::uint8_t value) = 0;
        // PC owner12C->5D / PS2 owner138->5D.
        virtual std::uint8_t ReadOwnerExitBlockForAnalysis(void* owner) = 0;
        // Lazy foreign manager59E090 /2A10F0. Captured once per effects pass.
        virtual void* RequireExitManagerForAnalysis() = 0;
        virtual std::uint8_t ReadExitManagerActiveForAnalysis(void* manager) = 0; // byte2C
        virtual void ClearExitManagerFlagForAnalysis(void* manager) = 0; // byte50
        virtual void* ReadExitManagerOwnerForAnalysis(void* manager) = 0; // pointer20
        // PC owner150 / PS2 owner15C.
        virtual std::uint8_t ReadExitOwnerActiveForAnalysis(void* owner) = 0;
        // PC owner144 / PS2 owner150.
        virtual void ClearExitOwnerFlagForAnalysis(void* owner) = 0;
    };
}
