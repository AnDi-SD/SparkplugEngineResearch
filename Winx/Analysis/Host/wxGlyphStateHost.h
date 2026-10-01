#pragma once

#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    class wxGlyphStateHost : public wxCharacterStateHost
    {
    public:
        // PC owner+124 -> word14C; PS2 owner+130 -> word158.
        // Full enum and original field name remain unknown.
        virtual std::uint32_t ReadOwnerClassificationWordForAnalysis(void* owner) = 0;
    };
}
