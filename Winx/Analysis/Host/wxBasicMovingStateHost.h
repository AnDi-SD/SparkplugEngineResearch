#pragma once

#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxBasicMovingState;

    class wxBasicMovingStateHost : public wxCharacterStateHost
    {
    public:
        // PC owner+124 -> motion+130; PS2 owner+130 -> motion+13C.
        virtual float MovementMagnitudeForAnalysis(void* owner) = 0;
        virtual bool MovementFlagForAnalysis(void* owner) = 0;
        // Selector 9 calls PC 513280 / PS2 2C89E0 before drawing from the
        // game's RNG, and PC 513300 / PS2 2C8870 on every exit.
        virtual void PrepareRandomMovementForAnalysis(wxBasicMovingState& state) = 0;
        // The caller owns RNG state and platform-specific float conversion.
        // Return the outcome of the original comparison with 0.5.
        virtual bool RandomMovementBelowHalfForAnalysis() = 0;
        virtual void FinishRandomMovementForAnalysis(wxBasicMovingState& state) = 0;
    };
}
