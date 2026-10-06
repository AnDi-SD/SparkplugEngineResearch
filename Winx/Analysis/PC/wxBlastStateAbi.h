#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::pc
{
    using wxBlastStateLayout = wxCharacterStateLayout;
    static_assert(sizeof(wxBlastStateLayout) == 0x3c);
    inline constexpr Address32 BlastTable = 0x006f8c48;
    inline constexpr Address32 BlastFactory = 0x00402ea0;
    inline constexpr Address32 BlastClone = 0x00409980;
    inline constexpr Address32 BlastEntry = 0x00518c60;
    inline constexpr Address32 BlastUpdate = 0x00523690;
    inline constexpr Address32 BlastPermission = 0x005203d0;
    inline constexpr Address32 BlastTag = 0x00518ae0;
}
