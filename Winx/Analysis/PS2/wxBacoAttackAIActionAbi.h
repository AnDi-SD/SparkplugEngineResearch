#pragma once

#include "Analysis/PS2/wxAIActionAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxBacoAttackAIActionLayout final
    {
        wxAIActionLayout base;           // 0x000
        Address32 target;                // 0x3ac, borrowed
        std::uint32_t state;             // 0x3b0
        std::uint32_t field3B4;          // 0x3b4
        std::uint32_t field3B8;          // 0x3b8, float bits on entry
        std::uint32_t field3BC;          // 0x3bc, constructor float bits
        std::uint8_t flag3C0;            // 0x3c0
        std::uint8_t unknown3C1[0x0F];  // constructor status not inferred from PC
    };

    static_assert(sizeof(wxBacoAttackAIActionLayout) == 0x3D0);
    static_assert(offsetof(wxBacoAttackAIActionLayout, target) == 0x3AC);
    static_assert(offsetof(wxBacoAttackAIActionLayout, flag3C0) == 0x3C0);
    inline constexpr std::uint32_t wxBacoAttackAIActionClassID = 0x013B1195;
    inline constexpr Address32 wxBacoAttackAIActionFactory = 0x00244C70;
    inline constexpr Address32 wxBacoAttackAIActionClone = 0x00244B80;
    inline constexpr Address32 wxBacoAttackAIActionCopy = 0x002446E0;
    inline constexpr Address32 wxBacoAttackAIActionRTTIGetter = 0x002440B0;
    inline constexpr Address32 wxBacoAttackAIActionVTable = 0x004923C0;
}
