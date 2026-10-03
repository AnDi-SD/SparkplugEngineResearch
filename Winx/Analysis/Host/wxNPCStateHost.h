#pragma once
#include "wxCharacterStateHost.h"
namespace winx::reconstruction
{
    class wxNPCStateHost : public wxCharacterStateHost
    {
    public:
        // PC owner+124 -> entity+130, PS2 owner+130 -> entity+13C.
        // Stored as a borrowed pointer in native state+40; no retain/free.
        virtual void* OwnerEntityField130ForAnalysis(void* owner) = 0;
        // PC owner+12C -> word+4, PS2 owner+138 -> word+4. Raw word,
        // used by MikaelWandring to write float0.1bits3DCCCCCD exactly.
        virtual void WriteOwnerActionControlForAnalysis(void* owner, std::uint32_t word) = 0;
    };
}
