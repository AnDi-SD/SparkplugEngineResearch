#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>

namespace winx::evidence::pc
{
    struct wxFrogAttackAIActionLayout final
    {
        wxAIActionLayout base;
        Address32 borrowedTarget3A8;
        std::uint8_t rawOwn3AC[60];
        Address32 borrowedCachedCommand3E8;
        std::uint8_t rawOwn3EC[4];
    };
    static_assert(sizeof(wxFrogAttackAIActionLayout) == 0x3F0);
    static_assert(offsetof(wxFrogAttackAIActionLayout, borrowedTarget3A8) == 0x3A8);
    static_assert(offsetof(wxFrogAttackAIActionLayout, rawOwn3AC) == 0x3AC);
    static_assert(offsetof(wxFrogAttackAIActionLayout, borrowedCachedCommand3E8) == 0x3E8);
    static_assert(offsetof(wxFrogAttackAIActionLayout, rawOwn3EC) == 0x3EC);
    inline constexpr std::uint32_t wxFrogAttackAIActionClassID = 0x4CFF056C;
    inline constexpr Address32 wxFrogAttackAIActionFactory = 0x5B3EF0;
    inline constexpr Address32 wxFrogAttackAIActionConstructor = 0x5B3D40;
    inline constexpr Address32 wxFrogAttackAIActionRTTIGetter = 0x5B3DD0;
    inline constexpr Address32 wxFrogAttackAIActionRTTIRecord = 0x767430;
    inline constexpr Address32 wxFrogAttackAIActionVTable = 0x709668;
    inline constexpr Address32 wxFrogAttackAIActionExit = 0x5B3ED0;
    inline constexpr Address32 wxFrogAttackAIActionEntry = 0x5B3E40;
    inline constexpr Address32 wxFrogAttackAIActionEntryCacheStore = 0x5B3E52;
}
