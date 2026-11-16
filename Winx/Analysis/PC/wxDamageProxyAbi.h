#pragma once
#include <cstddef>
#include <cstdint>

namespace wxDamageProxyAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x006F7F68;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x130;
inline constexpr std::size_t DamagePeriodRawOffset = 0x12C;
inline constexpr std::size_t DamagePointsRawOffset = 0x124;
inline constexpr std::uint32_t PeriodSetterPC = 0x005550A0;
inline constexpr std::uint32_t PeriodGetterPC = 0x005550B0;
inline constexpr std::uint32_t PointsSetterPC = 0x00555040;
inline constexpr std::uint32_t PointsGetterPC = 0x00555050;
// Historical factory allocation was312B. Native sizeof/current constructor remain unknown.
}
