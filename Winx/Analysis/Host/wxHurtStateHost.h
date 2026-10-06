#pragma once
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    enum class wxHurtStatePlatformForAnalysis { PC, PS2 };
    // Our borrowed owner graph reads. Accessors model plain native memory
    // reads, not new gameplay callbacks. The control pointer is captured once.
    class wxHurtStateHost : public wxCharacterStateHost
    {
    public:
        virtual wxHurtStatePlatformForAnalysis PlatformForAnalysis() const noexcept = 0;
        // PC owner124->130; PS2 owner130->13C, required on every entry.
        virtual void* OwnerHurtControlForAnalysis(void* owner) = 0;
        virtual std::uint8_t HurtControlByte60ForAnalysis(void* control) = 0;
        virtual float HurtControlMotionForAnalysis(void* control) = 0;
        // PC owner124->14C; PS2 owner130->158, required by slot38.
        virtual std::uint32_t OwnerModeForAnalysis(void* owner) = 0;
    };
}
