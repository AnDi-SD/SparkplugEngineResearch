#pragma once
#include "Analysis/PS2/wxAIActionAbi.h"
#include <array>
namespace winx::evidence::ps2
{
    struct wxIceGargoyleClawAIActionLayout final
    { wxAIActionLayout base;std::uint32_t word3A8,cachedNode,phase,stamp; };
    static_assert(sizeof(wxIceGargoyleClawAIActionLayout)==0x3BC);
    static_assert(offsetof(wxIceGargoyleClawAIActionLayout,cachedNode)==0x3B0);
    static_assert(offsetof(wxIceGargoyleClawAIActionLayout,phase)==0x3B4);
    static_assert(offsetof(wxIceGargoyleClawAIActionLayout,stamp)==0x3B8);
    inline constexpr std::uint32_t wxIceGargoyleClawFactory=0x2525E0;
    inline constexpr std::uint32_t wxIceGargoyleClawVtable=0x492810;
    inline constexpr std::uint32_t wxIceGargoyleClawRegistration=0x4BA910;
    // All own extra slots are retained; a17-slot prefix is incomplete here.
    // PS2 table starts with two zero header words, excluded below.
    inline constexpr std::array<std::uint32_t,21> wxIceGargoyleClawSlots{
        0x252490,0x252310,0x2524F0,0x252270,0x251F40,0x100010,0x100050,0x252420,0x2523F0,0x2234D0,0x252360,0x252330,0x252210,0x252320,0x223230,0x222F10,0x222E40,0x252160,0x252080,0x251FE0,0x251F50
    };
}

