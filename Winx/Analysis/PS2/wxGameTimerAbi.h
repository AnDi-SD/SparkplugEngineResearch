#pragma once

// Static physical layout. Constructor is inlined in native factory/clone;
// PC runtime proofs do not qualify the full PS2 EE constructor/pool path.
#include "Analysis/PS2/spMessageQueueAbi.h"
#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxGameTimerLayout final
    {
        std::uint8_t taskTimer[0x3C];
        std::uint32_t supportVTable;
        std::uint8_t paused, padding41[3];
        sparkplug::evidence::ps2::spMessageQueueLayout queue;
    };
    inline constexpr std::uint32_t wxGameTimerClassID = 0x78665312;
    inline constexpr std::uint32_t wxGameTimerFactory = 0x0028F250;
    inline constexpr std::uint32_t wxGameTimerClone = 0x0028F120;
    inline constexpr std::uint32_t wxGameTimerVTable = 0x004931E0;
    inline constexpr std::uint32_t wxGameTimerRegistration = 0x004C3360;
    inline constexpr std::uint32_t wxGameTimerDeletingDestructor = 0x0028F030;
    inline constexpr std::uint32_t wxGameTimerRegistrationGetter = 0x0028EFF0;
    inline constexpr std::uint32_t wxGameTimerStart = 0x0028F000;
    inline constexpr std::uint32_t wxGameTimerPause = 0x0028F010;
    inline constexpr std::uint32_t wxGameTimerReset = 0x00115240;
    static_assert(sizeof(wxGameTimerLayout) == 0x88);
    static_assert(offsetof(wxGameTimerLayout, paused) == 0x40);
    static_assert(offsetof(wxGameTimerLayout, queue) == 0x44);
    static_assert(offsetof(wxGameTimerLayout, queue)
        + offsetof(sparkplug::evidence::ps2::spMessageQueueLayout, borrowedTaskTimer) == 0x84);
}
