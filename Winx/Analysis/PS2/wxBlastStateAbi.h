#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::ps2
{
    using wxBlastStateLayout = wxCharacterStateLayout;
    static_assert(sizeof(wxBlastStateLayout) == 0x3c);
    inline constexpr Address32 BlastTable = 0x0049a120;
    inline constexpr Address32 BlastFactory = 0x003f35a0;
    inline constexpr Address32 BlastClone = 0x003f34e0;
    inline constexpr Address32 BlastConstructor = 0x002c81d0;
    inline constexpr Address32 BlastEntry = 0x002c7c50;
    inline constexpr Address32 BlastUpdate = 0x002c7c40;
    inline constexpr Address32 BlastPermission = 0x002c8140;
    inline constexpr Address32 BlastTag = 0x002c7ed0;
}
