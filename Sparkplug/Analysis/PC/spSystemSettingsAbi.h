#pragma once
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    struct SystemSettingsLayout32 final
    {
        std::uint8_t base[0x10];
        std::uint32_t singletonSupportVtable;
        std::uint8_t initializedOpaqueBytes[255];
        std::uint8_t unwrittenTrailingByte;
    };
    static_assert(sizeof(SystemSettingsLayout32) == 0x114);
    static_assert(offsetof(SystemSettingsLayout32, singletonSupportVtable) == 0x10);
    static_assert(offsetof(SystemSettingsLayout32, initializedOpaqueBytes) == 0x14);
    static_assert(offsetof(SystemSettingsLayout32, unwrittenTrailingByte) == 0x113);
    inline constexpr std::uint32_t SystemSettingsFactory = 0x4BE4F0;
    inline constexpr std::uint32_t SystemSettingsClone = 0x4BE580;
    inline constexpr std::uint32_t SystemSettingsDeletingDestructor = 0x4BE5D0;
    inline constexpr std::uint32_t SystemSettingsDestructor = 0x4BE5F0;
    inline constexpr std::uint32_t SystemSettingsRegistration = 0x764348;
    inline constexpr std::uint32_t SystemSettingsSingleton = 0x763A7C;
}
