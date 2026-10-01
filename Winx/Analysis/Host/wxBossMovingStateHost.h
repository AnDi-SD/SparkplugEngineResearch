#pragma once
#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxBossMovingStateHost : public wxCharacterStateHost
    {
    public:
        // PC owner+124 -> object+130; PS2 owner+130 -> object+13C.
        // Only float offsets4 and8 are read by this state's update.
        virtual float ReadOwnerMotionWordForAnalysis(void* owner, std::uint32_t offset) = 0;
    };
}
