#pragma once
#include "Analysis/PC/spMessageQueueAbi.h"
#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxGameTimerLayout final
    {
        sparkplug::evidence::pc::spTaskTimerLayout taskTimer;
        std::uint32_t supportVTable;
        std::uint8_t paused, padding41[3];
        sparkplug::evidence::pc::spMessageQueueLayout queue;
    };
    inline constexpr std::uint32_t wxGameTimerClassID = 0x78665312;
    inline constexpr std::uint32_t wxGameTimerFactory = 0x00596A90;
    inline constexpr std::uint32_t wxGameTimerConstructor = 0x00596970;
    inline constexpr std::uint32_t wxGameTimerDestructor = 0x00596A20;
    inline constexpr std::uint32_t wxGameTimerDeletingDestructor = 0x00596B40;
    inline constexpr std::uint32_t wxGameTimerClone = 0x00596AF0;
    inline constexpr std::uint32_t wxGameTimerVTable = 0x00702F94;
    inline constexpr std::uint32_t wxGameTimerRegistration = 0x007660E8;
    inline constexpr std::uint32_t wxGameTimerSingleton = 0x00755298;
    inline constexpr std::uint32_t wxGameTimerFlagGetter = 0x005968F0;
    inline constexpr std::uint32_t wxGameTimerStart = 0x00596910;
    inline constexpr std::uint32_t wxGameTimerPause = 0x00596900;
    inline constexpr std::uint32_t wxGameTimerReset = 0x004506E0;
    inline constexpr std::uint32_t wxGameTimerGlobalSpTimer = 0x0075F65C;
    static_assert(sizeof(wxGameTimerLayout) == 0x88);
    static_assert(offsetof(wxGameTimerLayout, supportVTable) == 0x3C);
    static_assert(offsetof(wxGameTimerLayout, paused) == 0x40);
    static_assert(offsetof(wxGameTimerLayout, queue) == 0x44);
    static_assert(offsetof(wxGameTimerLayout, queue)
        + offsetof(sparkplug::evidence::pc::spMessageQueueLayout, borrowedTaskTimer) == 0x84);
}
