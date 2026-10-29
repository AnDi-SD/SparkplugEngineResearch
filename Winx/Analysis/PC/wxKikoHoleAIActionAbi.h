#pragma once
#include "Analysis/PC/wxAIActionAbi.h"
#include <array>

namespace winx::evidence::pc
{
    struct wxKikoHoleAIActionLayout final
    {
        wxAIActionLayout base;
        std::uint32_t ownNode3A8;
        std::uint32_t playerNode3AC;
        std::uint32_t targetWord3B0;
        std::uint32_t phase3B4;
        std::uint32_t once3B8;
    };
    static_assert(sizeof(wxKikoHoleAIActionLayout) == 0x3BC);
    static_assert(offsetof(wxKikoHoleAIActionLayout, ownNode3A8) == 0x3A8);
    inline constexpr std::uint32_t wxKikoHoleAIActionFactory = 0x403F80;
    inline constexpr std::uint32_t wxKikoHoleAIActionVtable = 0x6F9D28;
    inline constexpr std::uint32_t wxKikoHoleAIActionRecord = 0x751008;
    inline constexpr std::uint32_t wxKikoHoleAIActionNotify = 0x522120;
    inline constexpr std::uint32_t wxKikoHoleAIActionUpdate = 0x5222B0;
    inline constexpr std::array<std::uint32_t, 17> wxKikoHoleAIActionSlots{
        0x5221A0,0x522120,0x40A790,0x58FDD0,0x5220A0,0x408350,
        0x408370,0x5222B0,0x4F3DF0,0x5221C0,0x5B7A00,0x58EA50,
        0x48EAA0,0x5A7DB0,0x518DC0,0x58F030,0x58F240};
}
