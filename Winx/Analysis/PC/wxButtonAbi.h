#pragma once
#include "Analysis/PC/spGUIAbi.h"

namespace winx::evidence::pc
{
    struct wxButtonLayout final
    {
        sparkplug::evidence::pc::spButtonLayout button;
        std::uint32_t word60;
        std::uint8_t strings[5][64];
        std::uint8_t refreshGate;
        std::uint8_t padding1A5[3];
    };
    static_assert(sizeof(wxButtonLayout)==0x1a8);
    static_assert(offsetof(wxButtonLayout,strings)==0x64);
    static_assert(offsetof(wxButtonLayout,refreshGate)==0x1a4);
    inline constexpr std::uint32_t wxButtonFactory=0x004067a0;
    inline constexpr std::uint32_t wxButtonConstructor=0x0055cab0;
    inline constexpr std::uint32_t wxButtonTable=0x006fe988;
    inline constexpr std::uint32_t wxButtonRecord=0x007538e8;
    inline constexpr std::uint32_t wxButtonSetState=0x0055ca70;
    inline constexpr std::uint32_t wxButtonRefresh=0x0055ce30;
    inline constexpr std::uint32_t wxButtonCopy=0x0055cc60;
    inline constexpr std::uint32_t wxButtonNotifyUnrecovered=0x0055cd30;
}
