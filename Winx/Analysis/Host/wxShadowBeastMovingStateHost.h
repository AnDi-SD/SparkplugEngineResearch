#pragma once
#include "wxCharacterMotionStateHost.h"

namespace winx::reconstruction
{
    class wxShadowBeastMovingStateHost : public wxCharacterMotionStateHost
    {
    public:
        // PC update reads byte1D of the borrowed control object on its
        // stationary branch. Its value does not affect the final request.
        virtual std::uint8_t ReadOwnerControlByteForAnalysis(void* owner,
            std::uint32_t offset) = 0;
    };
}
