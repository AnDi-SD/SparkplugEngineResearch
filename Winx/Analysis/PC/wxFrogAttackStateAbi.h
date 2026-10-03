#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::pc
{
    struct wxFrogAttackStateLayout final
    {
        wxCharacterStateLayout base;
        std::uint8_t byte3C,padding3D[3];
    };
    static_assert(sizeof(wxFrogAttackStateLayout)==0x40);
    static_assert(offsetof(wxFrogAttackStateLayout,byte3C)==0x3c);
    inline constexpr Address32 FrogAttackTable=0x006f98c8;
    inline constexpr Address32 FrogAttackFactory=0x00403a40;
    inline constexpr Address32 FrogAttackClone=0x0040a330;
    inline constexpr Address32 FrogAttackNotification=0x00520a40;
    inline constexpr Address32 FrogAttackEntry=0x00520b00;
    inline constexpr Address32 FrogAttackUpdate=0x00523690;
    inline constexpr Address32 FrogAttackPermission=0x00523770;
    inline constexpr Address32 FrogAttackTag=0x00520a90;
}
