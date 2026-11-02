#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>

namespace winx::evidence::pc
{
    struct wxYetiAttackAIActionLayout final
    {
        wxAIActionLayout base;
        Address32 borrowedTarget3A8;
        std::uint8_t rawOwnBytes3AC[232];
    };
    static_assert(sizeof(wxYetiAttackAIActionLayout) == 0x494);
    static_assert(offsetof(wxYetiAttackAIActionLayout, borrowedTarget3A8) == 0x3A8);
    static_assert(offsetof(wxYetiAttackAIActionLayout, rawOwnBytes3AC) == 0x3AC);
    inline constexpr std::uint32_t wxYetiAttackAIActionClassID = 0x30347478;
    inline constexpr Address32 wxYetiAttackAIActionFactory = 0x5B2120;
    inline constexpr Address32 wxYetiAttackAIActionConstructorPrefix = 0x5B0980;
    inline constexpr Address32 wxYetiAttackAIActionRTTIGetter = 0x5B0B30;
    inline constexpr Address32 wxYetiAttackAIActionRTTIRecord = 0x767370;
    inline constexpr Address32 wxYetiAttackAIActionVTable = 0x709418;
    inline constexpr Address32 wxYetiAttackAIActionSelectedExit = 0x5B79D0;
    inline constexpr Address32 wxYetiAttackAIActionOwnPhase54 = 0x5B0160;
    inline constexpr Address32 wxYetiAttackAIActionPhaseTimerSingleton = 0x755298;
}
