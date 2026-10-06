#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::ps2
{
    struct spInputDeviceObservedLayout final
    {
        std::array<std::uint8_t, 0x14> namedCrossPlatform;
        std::uint32_t inputInterfaceVtable;
        std::array<std::uint8_t, 0x0c> activeBindings;
        std::array<std::uint8_t, 0x0c> bindingGroups;
        std::array<std::uint32_t, 5> scratch;
    };
    struct spPS2InputDeviceObservedPrefix final
    {
        spInputDeviceObservedLayout base;
        std::uint8_t field44;
    };
    static_assert(sizeof(spInputDeviceObservedLayout) == 0x44);
    static_assert(offsetof(spPS2InputDeviceObservedPrefix, field44) == 0x44);
    // Five independently identified wrappers; PC slot4 has no independently
    // identified PS2 counterpart in these windows.
    inline constexpr std::uint32_t InputDeviceQuerySlot1 = 0x16CCE0;
    inline constexpr std::uint32_t InputDeviceQuerySlot2 = 0x16CBD0;
    inline constexpr std::uint32_t InputDeviceQuerySlot3 = 0x16CAE0;
    inline constexpr std::uint32_t InputDeviceCommandSlot5 = 0x16C9D0;
    inline constexpr std::uint32_t InputDeviceQuerySlot6 = 0x16C8E0;
    inline constexpr std::uint32_t InputDeviceConstructor = 0x16CFD0;
    inline constexpr std::uint32_t PS2InputDeviceConstructor = 0x1F0D20;
    inline constexpr std::uint32_t PS2InputDeviceDestructor = 0x1F0CB0;
}
