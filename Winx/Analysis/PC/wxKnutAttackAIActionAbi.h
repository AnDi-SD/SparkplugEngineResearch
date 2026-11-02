#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>

namespace winx::evidence::pc
{
    struct wxKnutAttackAIActionLayout final
    {
        wxAIActionLayout base;
        Address32 borrowedTarget3A8;
        std::uint8_t rawOwn3AC[92];
    };
    static_assert(sizeof(wxKnutAttackAIActionLayout) == 0x408);
    static_assert(offsetof(wxKnutAttackAIActionLayout, borrowedTarget3A8) == 0x3A8);
    static_assert(offsetof(wxKnutAttackAIActionLayout, rawOwn3AC) == 0x3AC);
    inline constexpr std::uint32_t wxKnutAttackAIActionClassID = 0x70D47C10;
    inline constexpr Address32 wxKnutAttackAIActionFactory = 0x5ACF60;
    inline constexpr Address32 wxKnutAttackAIActionConstructor = 0x5AC680;
    inline constexpr Address32 wxKnutAttackAIActionRTTIGetter = 0x5AC730;
    inline constexpr Address32 wxKnutAttackAIActionRTTIRecord = 0x7670D0;
    inline constexpr Address32 wxKnutAttackAIActionVTable = 0x708F58;
    inline constexpr Address32 wxKnutAttackAIActionExit = 0x5ABA70;
}
