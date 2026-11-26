#pragma once

#include "spInputDeviceAbi.h"

#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    inline constexpr std::uint32_t DXKeyboardClassId = 0x3A87729D;
    inline constexpr std::uint32_t DXKeyboardRegistrationBase = 0x2C2F3993;
    inline constexpr std::uint32_t DXKeyboardPrimary = 0x006F31D4;
    inline constexpr std::uint32_t DXKeyboardInterface = 0x006F31B8;
    inline constexpr std::size_t DXKeyboardInterfaceOffset =
        offsetof(spInputDeviceObservedLayout, inputInterfaceVtable);
    inline constexpr std::size_t DXKeyboardAcquiredOffset = offsetof(spDXInputDeviceObservedLayout, acquired45);
    inline constexpr std::size_t DXKeyboardCurrentSlot = 0x04;
    inline constexpr std::uint32_t DXKeyboardCurrentQuery = 0x004CC9D0;
    inline constexpr std::size_t DXKeyboardCurrentCacheOffset = 0x858;
    inline constexpr std::size_t DXKeyboardChangedSlot = 0x08;
    inline constexpr std::uint32_t DXKeyboardChangedQuery = 0x004CCA00;
    inline constexpr std::size_t DXKeyboardChangedCacheOffset = 0x958;
    inline constexpr std::size_t DXKeyboardCapturedPrefixBytes = 0xA2C;

    static_assert(DXKeyboardInterfaceOffset == 0x14 && DXKeyboardAcquiredOffset == 0x45);
}
