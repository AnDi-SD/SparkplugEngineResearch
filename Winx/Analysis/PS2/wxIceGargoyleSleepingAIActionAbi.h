#pragma once
#include "Analysis/PS2/wxAIActionAbi.h"
namespace winx::evidence::ps2
{
    struct wxIceGargoyleSleepingAIActionLayout final
    { wxAIActionLayout base; std::uint32_t phase, stamp; };
    static_assert(sizeof(wxIceGargoyleSleepingAIActionLayout) == 0x3B4);
    static_assert(offsetof(wxIceGargoyleSleepingAIActionLayout, stamp) == 0x3B0);
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionFactory = 0x252A90;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionVtable = 0x492870;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionTick = 0x2528F0;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionEntry = 0x2528D0;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionExit = 0x2528B0;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionWake = 0x252660;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionNotify = 0x252850;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionCopy = 0x2527C0;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionStage0 = 0x252750;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionStage1 = 0x252690;
}
