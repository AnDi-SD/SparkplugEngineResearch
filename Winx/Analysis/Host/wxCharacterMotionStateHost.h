#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    // Borrowed scalar from PC owner->field12C / PS2 owner->field138, +4.
    // Virtual inheritance permits one host to implement motion and speed.
    class wxCharacterMotionStateHost : public virtual wxCharacterStateHost
    {
    public:
        virtual float OwnerMotionForAnalysis(void* owner) = 0;
    };
}
