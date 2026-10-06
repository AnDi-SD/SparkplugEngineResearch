#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    struct spInputDeviceObservedLayout final
    {
        std::array<std::uint8_t, 0x14> namedCrossPlatform;
        std::uint32_t inputInterfaceVtable;
        std::array<std::uint8_t, 0x0c> activeBindings;
        std::array<std::uint8_t, 0x0c> bindingGroups;
        std::array<std::uint32_t, 5> scratch;
    };
    struct spDXInputDeviceObservedLayout final
    {
        spInputDeviceObservedLayout base;
        std::uint8_t field44;
        std::uint8_t acquired45;
        std::array<std::uint8_t, 2> padding46;
        std::uint32_t field48;
        std::uint32_t inputInterface4C;
        std::uint32_t device50;
        std::array<std::uint8_t, 0x2c> untouched54;
        std::uint32_t field80;
        std::uint8_t field84;
        std::array<std::uint8_t, 3> padding85;
    };
    static_assert(sizeof(spInputDeviceObservedLayout) == 0x44);
    static_assert(sizeof(spDXInputDeviceObservedLayout) == 0x88);
    static_assert(offsetof(spInputDeviceObservedLayout, scratch) == 0x30);
    static_assert(offsetof(spDXInputDeviceObservedLayout, inputInterface4C) == 0x4c);
    inline constexpr std::array<std::uint32_t, 6> InputDeviceQueries{
        0x4D67A0, 0x4D6800, 0x4D6860, 0x4D68B0, 0x4D6900, 0x4D6960};
    inline constexpr std::uint32_t InputDeviceAppendBinding = 0x4D69D0;
    inline constexpr std::uint32_t InputDeviceClearBindings = 0x4D66E0;
    inline constexpr std::uint32_t InputDeviceConstructor = 0x4D6D70;
    inline constexpr std::uint32_t DXInputDeviceConstructor = 0x4D64A0;
    inline constexpr std::uint32_t DXInputDeviceDestructor = 0x4D63E0;
}
