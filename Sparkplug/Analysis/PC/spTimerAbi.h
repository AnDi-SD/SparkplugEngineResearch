#pragma once

// Evidence layout for original PC and PS2 spTimer. Never overlay this structure
// on a portable spTimer object. Native padding remains untouched by methods.
#include <cstddef>
#include <cstdint>

namespace sparkplug::analysis::pc
{
    struct spTimerState32
    {
        std::uint32_t vtable, base04;
        std::uint16_t base08;
        std::uint8_t padding0A[2];
        std::uint32_t base0C;
        std::uint8_t active;
        std::uint8_t padding11[3];
        std::uint32_t accumulated, startedAt;
        std::uint8_t limited;
        std::uint8_t padding1D[3];
        std::uint32_t limit;
    };
    static_assert(sizeof(spTimerState32) == 0x24);
    static_assert(offsetof(spTimerState32, active) == 0x10);
    static_assert(offsetof(spTimerState32, accumulated) == 0x14);
    static_assert(offsetof(spTimerState32, startedAt) == 0x18);
    static_assert(offsetof(spTimerState32, limited) == 0x1C);
    static_assert(offsetof(spTimerState32, limit) == 0x20);
    inline constexpr std::uint32_t TimerConstructor = 0x006BE360;
    inline constexpr std::uint32_t TimerStart = 0x006BE2D0;
    inline constexpr std::uint32_t TimerStop = 0x006BE2F0;
    inline constexpr std::uint32_t TimerReset = 0x006BE3C0;
    inline constexpr std::uint32_t TimerFactory = 0x00481950;
    inline constexpr std::uint32_t TimerClone = 0x004819B0;
    inline constexpr std::uint32_t TimerDeletingDestructor = 0x006BE3A0;
    inline constexpr std::uint32_t TimerVtable = 0x007291B8;
    inline constexpr std::uint32_t TimerClockIat = 0x006D9454;
}
