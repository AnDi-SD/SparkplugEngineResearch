#pragma once
#include "Analysis/PC/wxEntityAbi.h"
#include <array>

namespace winx::evidence::pc
{
    struct wxCharacterMoveCtrlLayout final
    {
        wxEntityLayout base;
        std::uint8_t opaqueOwn124[0x1C18-0x124];
    };
    static_assert(sizeof(wxCharacterMoveCtrlLayout)==0x1C18);
    static_assert(offsetof(wxCharacterMoveCtrlLayout,opaqueOwn124)==0x124);
    inline constexpr std::uint32_t wxCharacterMoveCtrlFactory=0x401640;
    inline constexpr std::uint32_t wxCharacterMoveCtrlVtable=0x6F64F8;
    inline constexpr std::uint32_t wxCharacterMoveCtrlReset=0x4D96A0;
    inline constexpr std::array<std::uint32_t,15> wxCharacterMoveCtrlResetFields{
        0x1BC,0x1C0,0x1C4,0x1B0,0x1B4,0x1B8,0x1C8,0x1CC,0x1D0,0x194,0x198,0x19C,0x188,0x18C,0x190};
}
