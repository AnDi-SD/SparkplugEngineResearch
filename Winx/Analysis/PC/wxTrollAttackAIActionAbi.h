#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>

namespace winx::evidence::pc
{
    struct wxTrollAttackAIActionLayout final
    {
        wxAIActionLayout base;
        Address32 borrowedTarget3A8;
        std::uint8_t rawOwn3AC[164];
    };
    static_assert(sizeof(wxTrollAttackAIActionLayout) == 0x450);
    static_assert(offsetof(wxTrollAttackAIActionLayout, borrowedTarget3A8) == 0x3A8);
    static_assert(offsetof(wxTrollAttackAIActionLayout, rawOwn3AC) == 0x3AC);
    inline constexpr std::uint32_t wxTrollAttackAIActionClassID = 0x6A421178;
    inline constexpr Address32 wxTrollAttackAIActionFactory = 0x5B6F00;
    inline constexpr Address32 wxTrollAttackAIActionConstructor = 0x5B60C0;
    inline constexpr Address32 wxTrollAttackAIActionRTTIGetter = 0x5B61C0;
    inline constexpr Address32 wxTrollAttackAIActionRTTIRecord = 0x767550;
    inline constexpr Address32 wxTrollAttackAIActionVTable = 0x709840;
    inline constexpr Address32 wxTrollAttackAIActionExit = 0x5C2230;
}
