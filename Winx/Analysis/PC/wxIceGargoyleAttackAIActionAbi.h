#pragma once
#include "Analysis/PC/wxAIActionAbi.h"
#include <array>
namespace winx::evidence::pc
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
    static_assert(sizeof(wxIceGargoyleAttackAIActionLayout)==0x3E8);
    static_assert(offsetof(wxIceGargoyleAttackAIActionLayout,speed3C4)==0x3C4);
    static_assert(offsetof(wxIceGargoyleAttackAIActionLayout,cachedNode)==0x3D8);
    static_assert(offsetof(wxIceGargoyleAttackAIActionLayout,latch)==0x3E4);
    inline constexpr std::uint32_t wxIceGargoyleAttackFactory=0x5AF6E0;
    inline constexpr std::uint32_t wxIceGargoyleAttackVtable=0x709308;
    inline constexpr std::uint32_t wxIceGargoyleAttackRegistration=0x767310;
    // Full24 slots; PS2's two leading header words are excluded.
    inline constexpr std::array<std::uint32_t,24> wxIceGargoyleAttackSlots{
        0x5AF330,0x5AF350,0x5AF740,0x5AF370,0x5AF250,0x408350,0x408370,0x5AF790,0x4F3DF0,0x4F3DF0,0x5AF810,0x5AF260,0x5ADBC0,0x5A7DB0,0x518DC0,0x58F030,0x58F240,0x5AF630,0x5AF450,0x5AF530,0x5AF990,0x5AF870,0x5AFCC0,0x5AF5E0
    };
}
