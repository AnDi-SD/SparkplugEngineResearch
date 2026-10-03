#pragma once
#include "wxShadowBeastDefenseStateHost.h"
namespace winx::reconstruction
{
    // Our host interface shares the identical notification/controller seam.
    // It does not assert native inheritance between the two state classes.
    class wxMinotaurDefenseStateHost : public wxShadowBeastDefenseStateHost
    {
    public:
        // Borrowed PC owner12C / PS2 owner138. Retained across the motion
        // read and write; unknown owner/control source types stay opaque.
        virtual void* OwnerControlObjectForAnalysis(void* owner) = 0;
        virtual float ReadControlMotionForAnalysis(void* control) = 0;
        virtual void WriteControlWordForAnalysis(void* control, std::uint32_t bits) = 0;
    };
}
