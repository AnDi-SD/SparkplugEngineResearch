#pragma once
#include <cstddef>
#include <cstdint>

namespace wxThunderMakerAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x007020B4;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x130;
inline constexpr std::size_t MaxDelayRawOffset = 0x12C;
inline constexpr std::size_t MinDelayRawOffset = 0x128;
inline constexpr std::uint32_t MaxSetterPC = 0x005550A0;
inline constexpr std::uint32_t MaxGetterPC = 0x005550B0;
inline constexpr std::uint32_t MinSetterPC = 0x00555060;
inline constexpr std::uint32_t MinGetterPC = 0x00555070;
// Historical factory allocation was312B. Native sizeof/current constructor remain unknown.
}
