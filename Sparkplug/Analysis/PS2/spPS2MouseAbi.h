#pragma once
#include "spInputDeviceAbi.h"

namespace sparkplug::evidence::ps2
{
    inline constexpr std::uint32_t PS2MouseAllocationSize = 0xE0;
    inline constexpr std::uint32_t PS2MouseFactory = 0x1F1F10;
    inline constexpr std::uint32_t PS2MouseClone = 0x1F1E30;
    inline constexpr std::uint32_t PS2MouseDestructor = 0x1F1DC0;
    inline constexpr std::uint32_t PS2MousePrimary = 0x491660;
    inline constexpr std::uint32_t PS2MouseSecondary = 0x491684;
    inline constexpr std::uint32_t PS2MouseInitialize = 0x1F1D30;
    inline constexpr std::uint32_t PS2MousePoll = 0x1F1930;
    inline constexpr std::uint32_t PS2MouseUnidentifiedVoid = 0x1F1740;
    inline constexpr std::array<std::uint32_t,6> PS2MousePhysicalLeaves{
        0x1F18E0,0x1F1890,0x1F17E0,0x1F1770,0x1F1760,0x1F1750};
    inline constexpr std::array<std::uint32_t,6> PS2MousePhysicalThunks{
        0x1F2080,0x1F2070,0x1F2060,0x1F2050,0x1F2040,0x1F2030};
    inline constexpr std::int32_t PS2MouseSemaphoreGpOffset = -0x4558;
    inline constexpr std::int32_t PS2MouseInitializeGateGpOffset = -0x4554;
    inline constexpr std::uint32_t PS2MouseRpcClient = 0x4B7900;
    inline constexpr std::uint32_t PS2MouseRpcSend = 0x4B7940;
    inline constexpr std::uint32_t PS2MouseRpcCompletion = 0x1F1F80;
    inline constexpr std::uint32_t PS2MouseRpcInitialize = 0x1F1F90;
    // Factory writes common prefix and both tables only. Own tail48..DF and
    // native padding45..47 retain incoming heap bytes; they are not defaults.
}
