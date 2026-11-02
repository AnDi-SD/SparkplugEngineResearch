#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>

namespace winx::evidence::pc
{
    struct wxStormyAttackAIActionLayout final
    {
        wxAIActionLayout base;
        Address32 borrowedTarget3A8;
        std::uint8_t rawOwnBytes3AC[0xC4];
    };
    static_assert(sizeof(wxStormyAttackAIActionLayout) == 0x470);
    static_assert(offsetof(wxStormyAttackAIActionLayout, borrowedTarget3A8) == 0x3A8);
    static_assert(offsetof(wxStormyAttackAIActionLayout, rawOwnBytes3AC) == 0x3AC);
    inline constexpr std::uint32_t wxStormyAttackAIActionClassID = 0x32957C4C;
    inline constexpr Address32 wxStormyAttackAIActionFactory = 0x5C86B0;
    inline constexpr Address32 wxStormyAttackAIActionVTable = 0x70A440;
    inline constexpr Address32 wxStormyAttackAIActionRTTIGetter = 0x5C6450;
    inline constexpr Address32 wxStormyAttackAIActionRTTIRecord = 0x767A90;
    inline constexpr Address32 wxStormyAttackAIActionEntry = 0x5C56F0;
    inline constexpr Address32 wxStormyAttackAIActionExit = 0x5B79D0;
    inline constexpr Address32 wxStormyAttackAIActionRegistryCall = 0x5C570B;
    inline constexpr Address32 wxStormyAttackAIActionRegistryReturn = 0x5C5710;
}
