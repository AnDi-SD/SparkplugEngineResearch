#pragma once
#include "Analysis/PS2/wxAIActionAbi.h"
#include <array>
namespace winx::evidence::ps2
{
    struct wxIceGargoyleWithdrawlAIActionLayout final
    { wxAIActionLayout base;std::uint32_t word3A8,cachedNode,phase,stamp; };
    static_assert(sizeof(wxIceGargoyleWithdrawlAIActionLayout)==0x3BC);
    static_assert(offsetof(wxIceGargoyleWithdrawlAIActionLayout,cachedNode)==0x3B0);
    static_assert(offsetof(wxIceGargoyleWithdrawlAIActionLayout,phase)==0x3B4);
    static_assert(offsetof(wxIceGargoyleWithdrawlAIActionLayout,stamp)==0x3B8);
    inline constexpr std::uint32_t wxIceGargoyleWithdrawlFactory=0x2530F0;
    inline constexpr std::uint32_t wxIceGargoyleWithdrawlVtable=0x4928D0;
    inline constexpr std::uint32_t wxIceGargoyleWithdrawlRegistration=0x4BA9D0;
    // All own extra slots are retained; a17-slot prefix is incomplete here.
    // PS2 table starts with two zero header words, excluded below.
    inline constexpr std::array<std::uint32_t,20> wxIceGargoyleWithdrawlSlots{
        0x252FA0,0x252E40,0x253000,0x252DA0,0x252AF0,0x100010,0x100050,0x252F30,0x252F20,0x2234D0,0x252E90,0x252E60,0x252D40,0x252E50,0x223230,0x222F10,0x222E40,0x252C90,0x252BA0,0x252B00
    };
}

