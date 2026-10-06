#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::ps2
{
    using wxPhysicalAttackStateLayout = wxCharacterStateLayout;
    static_assert(sizeof(wxPhysicalAttackStateLayout) == 0x3C);
    inline constexpr Address32 PhysicalAttackTable = 0x0049A440;
    inline constexpr Address32 PhysicalAttackFactory = 0x003F3FA0;
    inline constexpr Address32 PhysicalAttackConstructor = 0x002D0EA0;
    inline constexpr Address32 PhysicalAttackEntry = 0x002D0B90;
    inline constexpr Address32 PhysicalAttackUpdate = 0x002D0CC0;
    inline constexpr Address32 PhysicalAttackPermission = 0x002D0E30;
    inline constexpr Address32 PhysicalAttackHook38 = 0x002D0E10;
    inline constexpr Address32 PhysicalAttackEvent = 0x002D0CD0;
}
