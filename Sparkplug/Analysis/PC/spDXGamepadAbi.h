#pragma once

#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    inline constexpr std::uint32_t DXGamepadClassId = 0x63663F48;
    inline constexpr std::uint32_t DXGamepadRegistrationBase = 0x2C2F3993;
    inline constexpr std::uint32_t DXGamepadPrimary = 0x006F30B4;
    inline constexpr std::uint32_t DXGamepadInterface = 0x006F3098;
    inline constexpr std::size_t DXGamepadInterfaceOffset = 0x14;
    inline constexpr std::size_t DXGamepadValueSlot = 0x0C;
    inline constexpr std::uint32_t DXGamepadValueQuery = 0x004CBA80;
    inline constexpr std::size_t DXGamepadActiveOffset = 0x44;
    inline constexpr std::size_t DXGamepadFirstButtonOffset = 0xB0;
    inline constexpr std::uint32_t DXGamepadFirstAxisSelector = 111;
    inline constexpr std::uint32_t DXGamepadFirstButtonSelector = 127;
    inline constexpr std::size_t DXGamepadButtonCount = 16;
} // namespace sparkplug::evidence::pc
