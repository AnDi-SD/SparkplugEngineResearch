#pragma once

#include <cstddef>
#include <cstdint>

namespace wxSpiderAIBehaviorAbi
{
    inline constexpr std::uint32_t ClassId = 0x64AB5890;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FF840;
    inline constexpr std::uint32_t OwnPublisherPC = 0x00564A60;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x1B1;
    inline constexpr std::size_t IsSpiderByteOffset = 0x1B0;
    inline constexpr std::uint32_t IsSpiderGetterPC = 0x005646A0;
    inline constexpr std::uint32_t IsSpiderSetterPC = 0x005646B0;
}
