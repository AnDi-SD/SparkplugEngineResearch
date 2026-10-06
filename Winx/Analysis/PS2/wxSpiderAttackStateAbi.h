#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::ps2
{
    using wxSpiderAttackStateLayout = wxCharacterStateLayout;
    static_assert(sizeof(wxSpiderAttackStateLayout) == 0x3c);
    inline constexpr Address32 SpiderAttackTable = 0x00499860;
    inline constexpr Address32 SpiderAttackFactory = 0x003f19a0;
    inline constexpr Address32 SpiderAttackClone = 0x003f18e0;
    inline constexpr Address32 SpiderAttackConstructor = 0x002f1cc0;
    inline constexpr Address32 SpiderAttackEntry = 0x002f18e0;
    inline constexpr Address32 SpiderAttackUpdate = 0x002f18d0;
    inline constexpr Address32 SpiderAttackPermission = 0x002f1c30;
    inline constexpr Address32 SpiderAttackTag = 0x002f1a70;
}
