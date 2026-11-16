#pragma once
#include <cstddef>
#include <cstdint>

namespace wxPendulumAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x007015BC;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x12C;
inline constexpr std::size_t PeriodRawOffset = 0x128;
inline constexpr std::size_t DamageRawOffset = 0x84;
inline constexpr std::uint32_t PeriodSetterPC = 0x00555060;
inline constexpr std::uint32_t PeriodGetterPC = 0x00555070;
inline constexpr std::uint32_t DamageSetterPC = 0x0057E390;
inline constexpr std::uint32_t DamageGetterPC = 0x00580BE0;
// Historical factory allocation was352B. Native sizeof/current constructor remain unknown.
}
