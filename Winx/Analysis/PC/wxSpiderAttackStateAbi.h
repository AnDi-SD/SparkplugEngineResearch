#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::pc
{
    using wxSpiderAttackStateLayout = wxCharacterStateLayout;
    static_assert(sizeof(wxSpiderAttackStateLayout) == 0x3c);
    inline constexpr Address32 SpiderAttackTable = 0x006f97d8;
    inline constexpr Address32 SpiderAttackFactory = 0x00403920;
    inline constexpr Address32 SpiderAttackClone = 0x0040a240;
    inline constexpr Address32 SpiderAttackEntry = 0x00520530;
    inline constexpr Address32 SpiderAttackUpdate = 0x00523690;
    inline constexpr Address32 SpiderAttackPermission = 0x005203d0;
    inline constexpr Address32 SpiderAttackTag = 0x00520410;
}
