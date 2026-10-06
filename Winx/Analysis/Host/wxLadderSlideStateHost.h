#pragma once
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    // Our borrowed owner graph accessor. The native code tests the low nibble
    // of PC owner+144 / PS2 owner+150; the original field's name is unknown.
    class wxLadderSlideStateHost : public wxCharacterStateHost
    {
    public:
        virtual std::uint8_t OwnerLadderExitFlagsForAnalysis(void* owner) = 0;
    };
}
