#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::ps2
{
    using wxIceWormAttackStateLayout = wxCharacterStateLayout;
    static_assert(sizeof(wxIceWormAttackStateLayout) == 0x3c);
    inline constexpr Address32 IceWormAttackTable = 0x00499f90;
    inline constexpr Address32 IceWormAttackFactory = 0x003f30a0;
    inline constexpr Address32 IceWormAttackClone = 0x003f2fe0;
    inline constexpr Address32 IceWormAttackConstructor = 0x002ec8b0;
    inline constexpr Address32 IceWormAttackEntry = 0x002ec430;
    inline constexpr Address32 IceWormAttackExit = 0x002ec250;
    inline constexpr Address32 IceWormAttackUpdate = 0x002ec5f0;
    inline constexpr Address32 IceWormAttackPermission = 0x002ec7d0;
    inline constexpr Address32 IceWormAttackTag = 0x002ec730;
}
