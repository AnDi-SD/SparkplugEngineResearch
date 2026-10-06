#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::ps2
{
    using wxOpenSecretPassageStateLayout=wxCharacterStateLayout;
    static_assert(sizeof(wxOpenSecretPassageStateLayout)==0x3c);
    inline constexpr Address32 OpenSecretPassageTable=0x004936a0;
    inline constexpr Address32 OpenSecretPassageFactory=0x002d0b30;
    inline constexpr Address32 OpenSecretPassageClone=0x002d0a50;
    inline constexpr Address32 OpenSecretPassageEntry=0x002d07c0;
    inline constexpr Address32 OpenSecretPassageUpdate=0x002d07b0;
    inline constexpr Address32 OpenSecretPassagePermission=0x002d0980;
    inline constexpr Address32 OpenSecretPassageTag=0x002d0910;
}
