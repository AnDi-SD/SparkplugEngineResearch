#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    // External borrowed fields; these accessors are our adapter API.
    class wxIceGargoyleMovingStateHost : public wxCharacterStateHost
    {
    public:
        // PC owner+140 / PS2 owner+14C: packed request bits, not a float.
        virtual std::uint32_t ReadOwnerActionKeyForAnalysis(void* owner) = 0;
        // PC owner+124 -> object+130, PS2 owner+130 -> object+13C; float+4.
        virtual float ReadOwnerMotionForAnalysis(void* owner) = 0;
    };
}
