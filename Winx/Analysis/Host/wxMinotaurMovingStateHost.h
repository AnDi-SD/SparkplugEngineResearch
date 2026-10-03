#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxMinotaurMovingStateHost : public wxCharacterStateHost
    {
    public:
        // Original PC owner12C / PS2 owner138, borrowed control object.
        virtual void* OwnerControlObjectForAnalysis(void* owner) = 0;
        virtual float ReadControlMotionForAnalysis(void* control) = 0;
        virtual void WriteControlWordForAnalysis(void* control, std::uint32_t bits) = 0;
        // PC rereads owner12C for the locked-animation write. This boundary
        // resolves the current owner's control rather than the captured one.
        virtual void WriteOwnerActionControlForAnalysis(void* owner, std::uint32_t bits) = 0;
        // PS2 retains the first motion in F1 for both comparisons. Qualified
        // only for normal finite float32 and signed zeros.
        [[nodiscard]] virtual bool UsePS2FiniteMotionProfileForAnalysis() const noexcept { return false; }
    };
}
