#pragma once

#include <cstddef>
#include <cstdint>

namespace wxSimpleDatingStateMachineAbi
{
    inline constexpr std::uint32_t ClassId = 0x0DDA040E;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FB340;
    inline constexpr std::uint32_t OwnPublisherPC = 0x00532EE0;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x234;
    inline constexpr std::size_t BloomOffset = 0x22C;
    inline constexpr std::size_t DateOffset = 0x230;
    inline constexpr std::uint32_t BloomGetterPC = 0x00532AF0;
    inline constexpr std::uint32_t BloomSetterPC = 0x00532B00;
    inline constexpr std::uint32_t DateGetterPC = 0x00532B10;
    inline constexpr std::uint32_t DateSetterPC = 0x00532B20;
}
