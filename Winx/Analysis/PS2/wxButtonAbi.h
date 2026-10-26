#pragma once
#include "Analysis/PS2/spGUIAbi.h"

namespace winx::evidence::ps2
{
    // Independent static metadata/layout. The current wholecall qualification
    // is PC only; native PS2 execution remains a separate gate.
    struct wxButtonLayout final
    {
        sparkplug::evidence::ps2::spButtonLayout button;
        std::uint32_t word5C;
        std::uint8_t strings[5][64];
        std::uint8_t refreshGate;
        std::uint8_t padding1A1[3];
    };
    static_assert(sizeof(wxButtonLayout)==0x1a4);
    static_assert(offsetof(wxButtonLayout,strings)==0x60);
    static_assert(offsetof(wxButtonLayout,refreshGate)==0x1a0);
    inline constexpr std::uint32_t wxButtonConstructor=0x003742e0;
    inline constexpr std::uint32_t wxButtonTable=0x00496900;
    inline constexpr std::uint32_t wxButtonRecord=0x004ca6d0;
    inline constexpr std::uint32_t wxButtonSetState=0x00373640;
    inline constexpr std::uint32_t wxButtonRefresh=0x00373690;
}
