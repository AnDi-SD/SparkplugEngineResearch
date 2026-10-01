#pragma once

#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxCrouchingStateHost : public wxCharacterStateHost
    {
    public:
        // PC owner+12C -> control+4; PS2 owner+138 -> control+4.
        virtual float CrouchingMotionForAnalysis(void* owner) = 0;
        // PC 513D2B calls 5A62B0 with zero through global7552A0->field1C;
        // PS2 2C9E78 calls372E00. The wider scene-service effect is external.
        virtual void SetEntryServiceByteForAnalysis(std::uint8_t value) = 0;
    };
}
