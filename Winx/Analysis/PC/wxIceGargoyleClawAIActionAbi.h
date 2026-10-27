#pragma once
#include "Analysis/PC/wxAIActionAbi.h"
#include <array>
namespace winx::evidence::pc
{
    struct wxIceGargoyleClawAIActionLayout final
    { wxAIActionLayout base;std::uint32_t word3A8,cachedNode,phase,stamp; };
    static_assert(sizeof(wxIceGargoyleClawAIActionLayout)==0x3B8);
    static_assert(offsetof(wxIceGargoyleClawAIActionLayout,cachedNode)==0x3AC);
    static_assert(offsetof(wxIceGargoyleClawAIActionLayout,phase)==0x3B0);
    static_assert(offsetof(wxIceGargoyleClawAIActionLayout,stamp)==0x3B4);
    inline constexpr std::uint32_t wxIceGargoyleClawFactory=0x5AEC30;
    inline constexpr std::uint32_t wxIceGargoyleClawVtable=0x7091F0;
    inline constexpr std::uint32_t wxIceGargoyleClawRegistration=0x767250;
    // All own extra slots are retained; a17-slot prefix is incomplete here.
    // PC table starts directly at slot0.
    inline constexpr std::array<std::uint32_t,21> wxIceGargoyleClawSlots{
        0x5AED10,0x5B7A00,0x5AECC0,0x5AED30,0x5AEBD0,0x408350,0x408370,0x5AE8C0,0x5AEBF0,0x4F3DF0,0x5AEE80,0x5AEC00,0x5ADBC0,0x5A7DB0,0x518DC0,0x58F030,0x58F240,0x5AEDB0,0x5AEEE0,0x5AEB40,0x5AEE30
    };
}

