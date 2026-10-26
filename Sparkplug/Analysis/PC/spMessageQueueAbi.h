#pragma once

// Original32-bit layouts, independent of the portable Queue/storage classes.
#include "SparkplugAbi.h"
#include <cstddef>

namespace sparkplug::evidence::pc
{
    struct spMessageQueueNotificationLayout final
    {
        std::uint32_t code;
        std::uint32_t due;
        std::uint32_t word08;
        std::uint32_t subscriptionKey;
        Address32 sender;
        Address32 receiver;
        std::uint32_t payload;
        std::uint32_t tail;
    };
    struct spMessageQueueNodeLayout final
    {
        spMessageQueueNotificationLayout notification;
        Address32 next;
    };
    struct spMessageQueueLayout final
    {
        spBaseObjectLayout base;
        // Same physical descriptor layout used by Animation; defaults differ:
        // four blocks,64 entries each,36-byte message nodes.
        AnimationDescriptorPoolLayout pool;
        Address32 head;
        Address32 borrowedTaskTimer;
    };
    inline constexpr std::uint32_t spMessageQueueClassID = 0xC32F87A0;
    inline constexpr Address32 spMessageQueueVTable = 0x006DB6DC;
    inline constexpr Address32 spMessageQueueRegistration = 0x0075A488;
    inline constexpr Address32 spMessageQueueConstructor = 0x00413C60;
    inline constexpr Address32 spMessageQueueDestructor = 0x00413CE0;
    inline constexpr Address32 spMessageQueueClear = 0x00413BB0;
    inline constexpr Address32 spMessageQueueEnqueue = 0x00413D80;
    inline constexpr Address32 spMessageQueueTick = 0x00413B40;
    static_assert(sizeof(spMessageQueueNotificationLayout) == 0x20);
    static_assert(offsetof(spMessageQueueNotificationLayout, receiver) == 0x14);
    static_assert(sizeof(spMessageQueueNodeLayout) == 0x24);
    static_assert(offsetof(spMessageQueueNodeLayout, next) == 0x20);
    static_assert(sizeof(spMessageQueueLayout) == 0x44);
    static_assert(offsetof(spMessageQueueLayout, pool) == 0x10);
    static_assert(offsetof(spMessageQueueLayout, head) == 0x3C);
    static_assert(offsetof(spMessageQueueLayout, borrowedTaskTimer) == 0x40);
}
