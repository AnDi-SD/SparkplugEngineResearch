#pragma once

#include <cstddef>
#include <cstdint>

namespace wxGhoulieAIBehaviorAbi
{
    inline constexpr std::uint32_t ClassId = 0x4BBA28D3;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FF338;
    inline constexpr std::uint32_t OwnPublisherPC = 0x00561C70;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x1C6;
    inline constexpr std::size_t IsInGardenia01Offset = 0x1C4;
    inline constexpr std::size_t JumpingGhoulieOffset = 0x1C5;
    inline constexpr std::uint32_t IsInGardenia01GetterPC = 0x00561040;
    inline constexpr std::uint32_t IsInGardenia01SetterPC = 0x00566AB0;
    inline constexpr std::uint32_t JumpingGhoulieGetterPC = 0x00561050;
    inline constexpr std::uint32_t JumpingGhoulieSetterPC = 0x00566AC0;
}
