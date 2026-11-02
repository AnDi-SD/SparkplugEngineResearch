#pragma once
#include "wxAIActionAbi.h"
#include <array>
#include <cstddef>

namespace winx::evidence::pc
{
    struct wxWanderAIActionLayout final
    {
        wxAIActionLayout base;
        std::array<std::uint32_t, 26> ownTail3A8;
    };
    static_assert(sizeof(wxWanderAIActionLayout) == 0x410);
    static_assert(offsetof(wxWanderAIActionLayout, ownTail3A8) == 0x3A8);
    inline constexpr std::uint32_t wxWanderAIActionClassID = 0x5E7865EA;
    inline constexpr Address32 wxWanderAIActionFactory = 0x5AB280;
    inline constexpr Address32 wxWanderAIActionVTable = 0x708ED8;
    inline constexpr Address32 wxWanderAIActionRTTIGetter = 0x5AB090;
    inline constexpr Address32 wxWanderAIActionRTTIRecord = 0x767070;
    inline constexpr Address32 wxWanderAIActionEntry = 0x5AB420;
    inline constexpr Address32 wxWanderAIActionDeletingDestructor = 0x5AB180;
}
