#pragma once

#include "Analysis/PC/wxAIActionAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxBacoAttackAIActionLayout final
    {
        wxAIActionLayout base;           // 0x000
        Address32 target;                // 0x3a8, borrowed
        std::uint32_t state;             // 0x3ac
        std::uint32_t field3B0;          // 0x3b0
        std::uint32_t field3B4;          // 0x3b4, float bits on entry
        std::uint32_t field3B8;          // 0x3b8, constructor float bits
        std::uint8_t flag3BC;            // 0x3bc
        std::uint8_t unknown3BD[0x0F];  // constructor does not initialize
    };

    static_assert(sizeof(wxBacoAttackAIActionLayout) == 0x3CC);
    static_assert(offsetof(wxBacoAttackAIActionLayout, target) == 0x3A8);
    static_assert(offsetof(wxBacoAttackAIActionLayout, flag3BC) == 0x3BC);
    inline constexpr std::uint32_t wxBacoAttackAIActionClassID = 0x013B1195;
    inline constexpr Address32 wxBacoAttackAIActionFactory = 0x005B82A0;
    inline constexpr Address32 wxBacoAttackAIActionClone = 0x005B8340;
    inline constexpr Address32 wxBacoAttackAIActionCopy = 0x005B8210;
    inline constexpr Address32 wxBacoAttackAIActionRTTIGetter = 0x005B8130;
    inline constexpr Address32 wxBacoAttackAIActionVTable = 0x007099E8;
}
