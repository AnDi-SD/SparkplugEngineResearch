#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::ps2
{
    struct wxFrogAttackStateLayout final
    {
        wxCharacterStateLayout base;
        std::uint8_t byte3C,padding3D[3];
    };
    static_assert(sizeof(wxFrogAttackStateLayout)==0x40);
    static_assert(offsetof(wxFrogAttackStateLayout,byte3C)==0x3c);
    inline constexpr Address32 FrogAttackTable=0x00499770;
    inline constexpr Address32 FrogAttackFactory=0x003f16a0;
    inline constexpr Address32 FrogAttackConstructor=0x002f3f70;
    inline constexpr Address32 FrogAttackNotification=0x002f3e40;
    inline constexpr Address32 FrogAttackEntry=0x002f3be0;
    inline constexpr Address32 FrogAttackUpdate=0x002f3bd0;
    inline constexpr Address32 FrogAttackPermission=0x002f3f00;
    inline constexpr Address32 FrogAttackTag=0x002f3da0;
}
