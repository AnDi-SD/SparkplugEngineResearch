#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>

namespace winx::evidence::pc
{
    struct wxMosquitoAttackAIActionLayout final
    {
        wxAIActionLayout base;
        Address32 borrowedTarget3A8;
        std::uint8_t rawOwnBytes3AC[32];
    };
    static_assert(sizeof(wxMosquitoAttackAIActionLayout)==0x3CC);
    static_assert(offsetof(wxMosquitoAttackAIActionLayout,borrowedTarget3A8)==0x3A8);
    static_assert(offsetof(wxMosquitoAttackAIActionLayout,rawOwnBytes3AC)==0x3AC);
    inline constexpr Address32 wxMosquitoAttackAIActionFactory=0x5B49A0;
    inline constexpr Address32 wxMosquitoAttackAIActionConstructor=0x5B48A0;
    inline constexpr Address32 wxMosquitoAttackAIActionVTable=0x7096D0;
    inline constexpr Address32 wxMosquitoAttackAIActionGetter=0x5B4900;
    inline constexpr Address32 wxMosquitoAttackAIActionRTTI=0x767490;
    inline constexpr Address32 wxMosquitoAttackAIActionExitCell=0x7096FC;
    inline constexpr Address32 wxMosquitoAttackAIActionExit=0x5B4920;
}
