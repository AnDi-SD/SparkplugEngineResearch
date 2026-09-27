#pragma once

#include "wxCharacterStateAbi.h"

namespace winx::evidence::pc
{
    struct wxAttackingStateLayout final
    {
        wxCharacterStateLayout base; // 0x00..0x3b
        std::uint8_t eventFlag;      // 0x3c (zero after PC factory)
        std::uint8_t padding3D[3];
    };

    static_assert(sizeof(wxAttackingStateLayout) == 0x40);
    static_assert(offsetof(wxAttackingStateLayout, eventFlag) == 0x3C);

    inline constexpr std::uint32_t wxAttackingStateClassID = 0x11B50C8E;
    inline constexpr Address32 wxAttackingStateFactory = 0x00402600;
    inline constexpr Address32 wxAttackingStateVTable = 0x006F8340;
    inline constexpr Address32 wxAttackingStateSlot1C = 0x00514210;
    inline constexpr Address32 wxAttackingStateSlot20 = 0x00514340;
    inline constexpr Address32 wxAttackingStateSlot30 = 0x005140A0;
    inline constexpr Address32 wxAttackingStateSlot34 = 0x00513E70;
    inline constexpr Address32 wxAttackingStateSlot38 = 0x005144F0;
    inline constexpr Address32 wxAttackingStateSlot3C = 0x00513FA0;
}
