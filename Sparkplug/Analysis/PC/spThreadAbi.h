#pragma once

// Physical32-bit PC allocation; portable C++ inheritance is independent.
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    struct spThreadObservedLayout final
    {
        std::uint32_t threadInterfaceVtable;
        std::array<std::uint8_t, 0x14> crossPlatformSubobject;
        std::uint32_t bodyParameter;
        std::uint8_t opaqueByte;
        std::array<std::uint8_t, 3> untouchedPadding;
    };
    struct spPCThreadObservedLayout final
    {
        spThreadObservedLayout base;
        std::uint32_t handle;
    };
    static_assert(sizeof(spThreadObservedLayout) == 0x20);
    static_assert(sizeof(spPCThreadObservedLayout) == 0x24);
    static_assert(offsetof(spThreadObservedLayout, crossPlatformSubobject) == 4);
    static_assert(offsetof(spThreadObservedLayout, bodyParameter) == 0x18);
    static_assert(offsetof(spThreadObservedLayout, opaqueByte) == 0x1C);
    static_assert(offsetof(spPCThreadObservedLayout, handle) == 0x20);
    inline constexpr std::uint32_t spPCThreadFactory = 0x6BE5C0;
    inline constexpr std::uint32_t spPCThreadCreate = 0x6BE490;
    inline constexpr std::uint32_t spPCThreadWait = 0x6BE4C0;
    inline constexpr std::uint32_t spPCThreadIsRunning = 0x6BE4F0;
    inline constexpr std::uint32_t spPCThreadResume = 0x6BE530;
    inline constexpr std::uint32_t spPCThreadSuspend = 0x6BE550;
    inline constexpr std::uint32_t spPCThreadSleep = 0x6BE5A0;
    inline constexpr std::uint32_t spPCThreadTerminate = 0x6BE570;
}
