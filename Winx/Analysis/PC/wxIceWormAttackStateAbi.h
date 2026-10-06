#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::pc
{
    using wxIceWormAttackStateLayout = wxCharacterStateLayout;
    static_assert(sizeof(wxIceWormAttackStateLayout) == 0x3c);
    inline constexpr Address32 IceWormAttackTable = 0x006f8e10;
    inline constexpr Address32 IceWormAttackFactory = 0x00403080;
    inline constexpr Address32 IceWormAttackClone = 0x00409b10;
    inline constexpr Address32 IceWormAttackEntry = 0x00519630;
    inline constexpr Address32 IceWormAttackExit = 0x00519700;
    inline constexpr Address32 IceWormAttackUpdate = 0x005195a0;
    inline constexpr Address32 IceWormAttackPermission = 0x005194d0;
    inline constexpr Address32 IceWormAttackTag = 0x00519530;
}
