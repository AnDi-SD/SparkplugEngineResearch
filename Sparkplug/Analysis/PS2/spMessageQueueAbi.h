#pragma once

// Static PS2 layouts/addresses. These notes do not qualify general EE pool/FPU
// execution or convert PC behavioral captures into PS2 dynamic evidence.
#include "SparkBaseAbi.h"
#include <cstddef>

namespace sparkplug::evidence::ps2
{
    struct spMessageQueuePoolLayout final
    {
        Address32 vtable;
        Address32 activeBlock;
        std::uint32_t entriesPerBlock;
        std::uint32_t field0C;
        std::uint32_t initialBlocks;
        std::uint32_t blockLimit;
        std::uint32_t entrySize;
        std::uint32_t blockCount;
        std::uint32_t freeEntryCount;
        Address32 spareBlock;
        std::uint32_t field28;
    };
    struct spMessageQueueNotificationLayout final
    {
        std::uint32_t code, due, word08, subscriptionKey;
        Address32 sender, receiver;
        std::uint32_t payload, tail;
    };
    struct spMessageQueueNodeLayout final
    {
        spMessageQueueNotificationLayout notification;
        Address32 next;
    };
    struct spMessageQueueLayout final
    {
        spBaseObjectLayout base;
        spMessageQueuePoolLayout pool;
        Address32 head;
        Address32 borrowedTaskTimer;
    };
    inline constexpr std::uint32_t spMessageQueueClassID = 0xC32F87A0;
    inline constexpr Address32 spMessageQueueVTable = 0x0048C830;
    inline constexpr Address32 spMessageQueueRegistration = 0x004A2410;
    inline constexpr Address32 spMessageQueueConstructor = 0x0010DE00;
    inline constexpr Address32 spMessageQueueDestructor = 0x0010DD40;
    inline constexpr Address32 spMessageQueueClear = 0x0010DA90;
    inline constexpr Address32 spMessageQueueEnqueue = 0x0010DBD0;
    inline constexpr Address32 spMessageQueueTick = 0x0010DAF0;
    static_assert(sizeof(spMessageQueuePoolLayout) == 0x2C);
    static_assert(sizeof(spMessageQueueNotificationLayout) == 0x20);
    static_assert(offsetof(spMessageQueueNotificationLayout, receiver) == 0x14);
    static_assert(sizeof(spMessageQueueNodeLayout) == 0x24);
    static_assert(offsetof(spMessageQueueNodeLayout, next) == 0x20);
    static_assert(sizeof(spMessageQueueLayout) == 0x44);
    static_assert(offsetof(spMessageQueueLayout, head) == 0x3C);
    static_assert(offsetof(spMessageQueueLayout, borrowedTaskTimer) == 0x40);
}
