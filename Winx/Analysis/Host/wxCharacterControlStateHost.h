#pragma once

#include "wxCharacterStateHost.h"

namespace winx::reconstruction
{
    // Adapter for the external movement/control object at owner+12C on PC
    // and owner+138 on PS2. Offsets refer to that object, not to the owner.
    // Source-level names and wider meaning of bytes 1A/60/61 remain unknown.
    class wxCharacterControlStateHost : public wxCharacterStateHost
    {
    public:
        virtual void ClearOwnerControlByteForAnalysis(void* owner,
            std::uint32_t offset) = 0;
    };
}
