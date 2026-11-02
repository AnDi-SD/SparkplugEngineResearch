#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>

namespace winx::evidence::pc
{
    struct wxGolemAttackAIActionLayout final
    {
        wxAIActionLayout base;
        Address32 borrowedTarget3A8;
        std::uint8_t rawOwnBytes3AC[20];
    };
    static_assert(sizeof(wxGolemAttackAIActionLayout) == 0x3C0);
    static_assert(offsetof(wxGolemAttackAIActionLayout, borrowedTarget3A8) == 0x3A8);
    static_assert(offsetof(wxGolemAttackAIActionLayout, rawOwnBytes3AC) == 0x3AC);
    inline constexpr std::uint32_t wxGolemAttackAIActionClassID = 0x4A405186;
    inline constexpr Address32 wxGolemAttackAIActionFactory = 0x5B7A90;
    inline constexpr Address32 wxGolemAttackAIActionRTTIGetter = 0x5B79B0;
    inline constexpr Address32 wxGolemAttackAIActionRTTIRecord = 0x767610;
    inline constexpr Address32 wxGolemAttackAIActionVTable = 0x709980;
    inline constexpr Address32 wxGolemAttackAIActionSelectedExit = 0x5B79D0;
    inline constexpr Address32 wxGolemAttackAIActionEntryCell = 0x7099A8;
    inline constexpr Address32 wxGolemAttackAIActionEntry = 0x5B7B90;
    inline constexpr Address32 wxGolemAttackAIActionEntryRegistryGlobal = 0x765AD8;
    inline constexpr Address32 wxGolemAttackAIActionFactoryBaseCall = 0x5B7ACE;
}
