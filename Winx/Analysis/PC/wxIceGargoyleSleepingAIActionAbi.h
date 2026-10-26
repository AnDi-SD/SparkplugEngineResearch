#pragma once
#include "Analysis/PC/wxAIActionAbi.h"
namespace winx::evidence::pc
{
    struct wxIceGargoyleSleepingAIActionLayout final
    { wxAIActionLayout base; std::uint32_t phase, stamp; };
    static_assert(sizeof(wxIceGargoyleSleepingAIActionLayout) == 0x3B0);
    static_assert(offsetof(wxIceGargoyleSleepingAIActionLayout, stamp) == 0x3AC);
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionFactory = 0x5AF010;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionVtable = 0x709268;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionTick = 0x5AEF90;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionEntry = 0x5AEFB0;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionExit = 0x5AEFD0;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionWake = 0x5AEFF0;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionNotify = 0x5AF100;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionCopy = 0x5AF120;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionStage0 = 0x5AF180;
    inline constexpr std::uint32_t wxIceGargoyleSleepingAIActionStage1 = 0x5AF1C0;
}
