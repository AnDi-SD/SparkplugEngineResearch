#pragma once
#include "spInputDeviceAbi.h"

namespace sparkplug::evidence::ps2
{
    inline constexpr std::uint32_t PS2KeyboardAllocationSize = 0x50;
    inline constexpr std::uint32_t PS2KeyboardFactory = 0x1F1650;
    inline constexpr std::uint32_t PS2KeyboardClone = 0x1F1570;
    inline constexpr std::uint32_t PS2KeyboardDestructor = 0x1F1500;
    inline constexpr std::uint32_t PS2KeyboardPrimary = 0x4915E0;
    inline constexpr std::uint32_t PS2KeyboardSecondary = 0x491604;
    inline constexpr std::uint32_t PS2KeyboardInitialize = 0x1F14F0;
    inline constexpr std::uint32_t PS2KeyboardReady = 0x1F14E0;
    inline constexpr std::array<std::uint32_t, 6> PS2KeyboardPhysicalLeaves{
        0x1F14D0, 0x1F14C0, 0x1F14B0, 0x1F1490, 0x1F14A0, 0x1F1480};
    inline constexpr std::array<std::uint32_t, 6> PS2KeyboardPhysicalThunks{
        0x1F1710, 0x1F1700, 0x1F16F0, 0x1F16E0, 0x1F16D0, 0x1F16C0};
    // Only the common prefix through +44 is initialized by the factory's base
    // constructor. Native bytes +45..+4F remain unspecified, not host padding.
}
