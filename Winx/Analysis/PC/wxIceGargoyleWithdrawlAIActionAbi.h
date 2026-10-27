#pragma once
#include "Analysis/PC/wxAIActionAbi.h"
#include <array>
namespace winx::evidence::pc
{
    struct wxIceGargoyleWithdrawlAIActionLayout final
    { wxAIActionLayout base;std::uint32_t word3A8,cachedNode,phase,stamp; };
    static_assert(sizeof(wxIceGargoyleWithdrawlAIActionLayout)==0x3B8);
    static_assert(offsetof(wxIceGargoyleWithdrawlAIActionLayout,cachedNode)==0x3AC);
    static_assert(offsetof(wxIceGargoyleWithdrawlAIActionLayout,phase)==0x3B0);
    static_assert(offsetof(wxIceGargoyleWithdrawlAIActionLayout,stamp)==0x3B4);
    inline constexpr std::uint32_t wxIceGargoyleWithdrawlFactory=0x5AE8F0;
    inline constexpr std::uint32_t wxIceGargoyleWithdrawlVtable=0x709170;
    inline constexpr std::uint32_t wxIceGargoyleWithdrawlRegistration=0x7671F0;
    // All own extra slots are retained; a17-slot prefix is incomplete here.
    // PC table starts directly at slot0.
    inline constexpr std::array<std::uint32_t,20> wxIceGargoyleWithdrawlSlots{
        0x5AE9D0,0x5B7A00,0x5AE980,0x5AE9F0,0x5AE8A0,0x408350,0x408370,0x5AE8C0,0x4F3DF0,0x4F3DF0,0x5AEE80,0x5AEC00,0x5ADBC0,0x5A7DB0,0x518DC0,0x58F030,0x58F240,0x5AEDB0,0x5AEA70,0x5AEB40
    };
}

