#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::pc
{
    using wxPhysicalAttackStateLayout = wxCharacterStateLayout;
    static_assert(sizeof(wxPhysicalAttackStateLayout) == 0x3C);
    inline constexpr Address32 PhysicalAttackTable = 0x006F8798;
    inline constexpr Address32 PhysicalAttackFactory = 0x00402AE0;
    inline constexpr Address32 PhysicalAttackEntry = 0x005176C0;
    inline constexpr Address32 PhysicalAttackUpdate = 0x00523690;
    inline constexpr Address32 PhysicalAttackPermission = 0x00523770;
    inline constexpr Address32 PhysicalAttackHook38 = 0x005175E0;
    inline constexpr Address32 PhysicalAttackEvent = 0x00517610;
}
