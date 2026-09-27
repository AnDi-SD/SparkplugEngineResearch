#pragma once

#include "wxCharacterStateAbi.h"

namespace winx::evidence::ps2
{
    struct wxAttackingStateLayout final
    {
        wxCharacterStateLayout base; // 0x00..0x3b
        std::uint8_t eventFlag;      // 0x3c
        std::uint8_t padding3D[3];
    };

    static_assert(sizeof(wxAttackingStateLayout) == 0x40);
    static_assert(offsetof(wxAttackingStateLayout, eventFlag) == 0x3C);

    inline constexpr std::uint32_t wxAttackingStateClassID = 0x11B50C8E;
    inline constexpr Address32 wxAttackingStateFactory = 0x003F4CA0;
    inline constexpr Address32 wxAttackingStateConstructor = 0x002C77A0;
    inline constexpr Address32 wxAttackingStateVTable = 0x0049A850;
    inline constexpr Address32 wxAttackingStateSlot1C = 0x002C7050;
    inline constexpr Address32 wxAttackingStateSlot20 = 0x002C6E50;
    inline constexpr Address32 wxAttackingStateSlot30 = 0x002C7260;
    inline constexpr Address32 wxAttackingStateSlot34 = 0x002C76F0;
    inline constexpr Address32 wxAttackingStateSlot38 = 0x002C76A0;
    inline constexpr Address32 wxAttackingStateSlot3C = 0x002C7510;
}
