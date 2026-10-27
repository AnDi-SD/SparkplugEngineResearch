#pragma once
#include "Analysis/PS2/wxAIActionAbi.h"
#include <array>
namespace winx::evidence::ps2
{
    struct wxIceGargoyleAttackAIActionLayout final
    {
        wxAIActionLayout base;
        std::uint32_t waypoint[3];std::uint8_t toggle,padding3B5[3];
        std::uint32_t direction[3],speed3C4,word3C8,word3CC;
        std::uint8_t byte3D0,byte3D1,byte3D2,padding3D3;
        std::uint32_t threshold,cachedNode,phase,time;
        std::uint8_t latch,padding3E5[3];
    };
    static_assert(sizeof(wxIceGargoyleAttackAIActionLayout)==0x3EC);
    static_assert(offsetof(wxIceGargoyleAttackAIActionLayout,speed3C4)==0x3C8);
    static_assert(offsetof(wxIceGargoyleAttackAIActionLayout,cachedNode)==0x3DC);
    static_assert(offsetof(wxIceGargoyleAttackAIActionLayout,latch)==0x3E8);
    inline constexpr std::uint32_t wxIceGargoyleAttackFactory=0x251F00;
    inline constexpr std::uint32_t wxIceGargoyleAttackVtable=0x4927A0;
    inline constexpr std::uint32_t wxIceGargoyleAttackRegistration=0x4BA8B0;
    // Full24 slots; PS2's two leading header words are excluded.
    inline constexpr std::array<std::uint32_t,24> wxIceGargoyleAttackSlots{
        0x251D40,0x251B20,0x251E40,0x251A40,0x250C70,0x100010,0x100050,0x251C50,0x251C40,0x2234D0,0x251BB0,0x251B80,0x2519E0,0x251B70,0x223230,0x222F10,0x222E40,0x250C80,0x251890,0x251770,0x251140,0x251600,0x250DE0,0x250D60
    };
}
