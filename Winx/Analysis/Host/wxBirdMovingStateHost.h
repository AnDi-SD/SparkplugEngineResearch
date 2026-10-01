#pragma once

#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    // Our adapter for borrowed game RNG and event/character objects.
    class wxBirdMovingStateHost : public wxCharacterStateHost
    {
    public:
        // Original RNG call PC4132B0 / PS2108350. No replacement RNG here.
        virtual std::uint32_t DrawRandomForAnalysis() = 0;
        // Native event+1C -> tag+10 holds a valid zero-terminated name.
        virtual const char* EventTagNameForAnalysis(const void* event) = 0;
        virtual void JumpFromEventForAnalysis(void* owner) = 0; // PC5296C0 / PS22E6D00
        virtual void LandFromEventForAnalysis(void* owner) = 0; // PC529090 / PS22E6CF0
    };
}
