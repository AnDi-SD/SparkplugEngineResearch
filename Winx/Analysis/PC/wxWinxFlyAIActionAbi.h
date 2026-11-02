#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>

namespace winx::evidence::pc
{
    struct wxWinxFlyAIActionLayout final
    {
        wxAIActionLayout base;
        Address32 borrowedTarget3A8;
        std::uint8_t rawOwnBytes3AC[0x14];
    };
    static_assert(sizeof(wxWinxFlyAIActionLayout) == 0x3C0);
    static_assert(offsetof(wxWinxFlyAIActionLayout, borrowedTarget3A8) == 0x3A8);
    static_assert(offsetof(wxWinxFlyAIActionLayout, rawOwnBytes3AC) == 0x3AC);
    inline constexpr std::uint32_t wxWinxFlyAIActionClassID = 0x67AF3520;
    inline constexpr Address32 wxWinxFlyAIActionFactory = 0x5BC5A0;
    inline constexpr Address32 wxWinxFlyAIActionVTable = 0x709DD8;
    inline constexpr Address32 wxWinxFlyAIActionRTTIGetter = 0x5BC580;
    inline constexpr Address32 wxWinxFlyAIActionRTTIRecord = 0x7678B0;
    inline constexpr Address32 wxWinxFlyAIActionEntry = 0x5BC6A0;
    inline constexpr Address32 wxWinxFlyAIActionRegistryCall = 0x5BC6D1;
    inline constexpr Address32 wxWinxFlyAIActionRegistryReturn = 0x5BC6D6;
    inline constexpr Address32 wxWinxFlyAIActionDeletingDestructor = 0x5BC680;
}
