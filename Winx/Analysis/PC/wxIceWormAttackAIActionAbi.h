#pragma once
#include "wxAIActionAbi.h"
#include <cstddef>

namespace winx::evidence::pc
{
    struct wxIceWormAttackAIActionLayout final
    {
        wxAIActionLayout base;
        Address32 borrowedTarget3A8;
        std::uint8_t rawOwn3AC[36];
    };
    static_assert(sizeof(wxIceWormAttackAIActionLayout) == 0x3D0);
    static_assert(offsetof(wxIceWormAttackAIActionLayout, borrowedTarget3A8) == 0x3A8);
    static_assert(offsetof(wxIceWormAttackAIActionLayout, rawOwn3AC) == 0x3AC);
    inline constexpr std::uint32_t wxIceWormAttackAIActionClassID = 0x76850CC0;
    inline constexpr Address32 wxIceWormAttackAIActionFactory = 0x5ADFA0;
    inline constexpr Address32 wxIceWormAttackAIActionConstructor = 0x5ADC80;
    inline constexpr Address32 wxIceWormAttackAIActionRTTIGetter = 0x5ADAF0;
    inline constexpr Address32 wxIceWormAttackAIActionRTTIRecord = 0x767190;
    inline constexpr Address32 wxIceWormAttackAIActionVTable = 0x7090E8;
    inline constexpr Address32 wxIceWormAttackAIActionExit = 0x5ADB90;
}
