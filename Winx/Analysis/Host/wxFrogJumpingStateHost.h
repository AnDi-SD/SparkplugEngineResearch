#pragma once
#include "wxCharacterStateHost.h"
#include <array>

namespace winx::reconstruction
{
    // Our borrowed projection of entity action-control fields, not native ABI.
    struct wxFrogJumpControlForAnalysis final
    {
        std::array<float, 3> velocity{};
        bool enabled = false;
    };

    class wxFrogJumpingStateHost : public wxCharacterStateHost
    {
    public:
        // event+1C -> tag+10, borrowed zero-terminated string.
        virtual const char* EventTagNameForAnalysis(const void* event) = 0;
        // PC owner+124 -> entity+12C; PS2 owner+130 -> entity+138.
        // The returned view must remain valid for the complete event call.
        virtual wxFrogJumpControlForAnalysis& OwnerEntityJumpControlForAnalysis(
            void* owner) = 0;
    };
}
