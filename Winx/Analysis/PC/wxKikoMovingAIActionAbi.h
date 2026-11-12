#pragma once
#include "wxAIActionAbi.h"
#include <array>

namespace winx::evidence::pc
{
    // Evidence-only native prefix, distinct from the portable C++ facade.
    struct wxKikoMovingAIActionLayout final
    { wxAIActionLayout base;std::array<std::uint8_t,7160-0x3A8> rawOwn3A8; };
    static_assert(sizeof(wxKikoMovingAIActionLayout)==7160);
    static_assert(offsetof(wxKikoMovingAIActionLayout,rawOwn3A8)==0x3A8);
    inline constexpr std::uint32_t wxKikoMovingAIActionClassID=0x99887766;
    inline constexpr Address32 wxKikoMovingAIActionPrimary=0x00709070;
    inline constexpr Address32 wxKikoMovingAIActionFactory=0x005AD1B0;
    inline constexpr Address32 wxKikoMovingAIActionConstructor=0x005AD030;
    inline constexpr Address32 wxKikoMovingAIActionRTTIGetter=0x005AD0F0;
    inline constexpr Address32 wxKikoMovingAIActionRTTIRecord=0x00767130;
    inline constexpr Address32 wxKikoMovingAIActionMessageHandler=0x005AD180;
    inline constexpr std::size_t wxKikoMovingAIActionMessageHandlerBytes=35;
    inline constexpr std::size_t wxKikoMovingAIActionMessageReadBytes=4;
    inline constexpr std::size_t wxKikoMovingAIActionOwnWord3A8=0x3A8;
    inline constexpr std::size_t wxKikoMovingAIActionOwnWord3AC=0x3AC;
    // No native message-class layout or original C++ return signature asserted.
}
