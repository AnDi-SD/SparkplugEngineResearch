#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::pc
{
    using wxOpenSecretPassageStateLayout=wxCharacterStateLayout;
    static_assert(sizeof(wxOpenSecretPassageStateLayout)==0x3c);
    inline constexpr Address32 OpenSecretPassageTable=0x00706780;
    inline constexpr Address32 OpenSecretPassageFactory=0x005a6370;
    inline constexpr Address32 OpenSecretPassageClone=0x005a63e0;
    inline constexpr Address32 OpenSecretPassageEntry=0x005a64a0;
    inline constexpr Address32 OpenSecretPassageUpdate=0x00523690;
    inline constexpr Address32 OpenSecretPassagePermission=0x005a6770;
    inline constexpr Address32 OpenSecretPassageTag=0x005a6450;
}
