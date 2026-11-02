#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>

namespace winx::evidence::pc
{
    // The direct same-receiver58FF00 constructor prefix establishes the
    // AIAction prefix. This layout is separate from the host C++ facade.
    struct wxDarcyAttackAIActionLayout final
    {
        wxAIActionLayout base;
        Address32 borrowedTarget3A8;
        std::uint8_t rawOwnBytes3AC[0xB4];
    };
    static_assert(sizeof(wxDarcyAttackAIActionLayout) == 0x460);
    static_assert(offsetof(wxDarcyAttackAIActionLayout, borrowedTarget3A8) == 0x3A8);
    static_assert(offsetof(wxDarcyAttackAIActionLayout, rawOwnBytes3AC) == 0x3AC);
    inline constexpr std::uint32_t wxDarcyAttackAIActionClassID = 0x590C5D10;
    inline constexpr Address32 wxDarcyAttackAIActionHistoricalFactory = 0x5C4B40;
    inline constexpr Address32 wxDarcyAttackAIActionVTable = 0x70A230;
    inline constexpr Address32 wxDarcyAttackAIActionRTTIGetter = 0x5C2D20;
    inline constexpr Address32 wxDarcyAttackAIActionRTTIRecord = 0x767A30;
    inline constexpr Address32 wxDarcyAttackAIActionEntry = 0x5C23D0;
    inline constexpr Address32 wxDarcyAttackAIActionExit = 0x5C2230;
    inline constexpr Address32 wxDarcyAttackAIActionRegistryCall = 0x5C23EB;
    inline constexpr Address32 wxDarcyAttackAIActionRegistryReturn = 0x5C23F0;
}
