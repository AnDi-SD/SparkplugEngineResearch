#pragma once

#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxBasicMovingStateHost : public wxCharacterStateHost
    {
    public:
        // PC owner+124 -> motion+130; PS2 owner+130 -> motion+13C.
        virtual float MovementMagnitudeForAnalysis(void* owner) = 0;
        virtual bool MovementFlagForAnalysis(void* owner) = 0;
    };
}
