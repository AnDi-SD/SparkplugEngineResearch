#pragma once

#include <cstddef>
#include <cstdint>

namespace wxMinotaurAIBehaviorAbi
{
    inline constexpr std::uint32_t ClassId = 0x4E4C79B0;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FFC48;
    inline constexpr std::uint32_t OwnPublisherPC = 0x00567AF0;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x1BA;
    inline constexpr std::size_t IdOffset = 0x1B0;
    inline constexpr std::uint32_t IdGetterPC = 0x0054ED20;
    inline constexpr std::uint32_t IdSetterPC = 0x00563230;
    inline constexpr std::size_t EnergyOffset = 0x1B4;
    inline constexpr std::uint32_t EnergyGetterPC = 0x004FD790;
    inline constexpr std::uint32_t EnergySetterPC = 0x00563240;
    inline constexpr std::size_t Ct2Offset = 0x1B9;
    inline constexpr std::uint32_t Ct2GetterPC = 0x00567700;
    inline constexpr std::uint32_t Ct2SetterPC = 0x00567710;
}
