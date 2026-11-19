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

// Separate finite F32 observation prefix; existing RAW constants above retain their contract.
namespace wxPendulumAbi
{
inline constexpr std::size_t CapturedScalarMinimumPrefixBytes = 0x154;
inline constexpr std::size_t MaximumAngleOffset = 0x124;
inline constexpr std::size_t InitialOffsetOffset = 0x150;
inline constexpr std::uint32_t MaximumAngleSetterPC = 0x00555040;
inline constexpr std::uint32_t MaximumAngleGetterPC = 0x00556E30;
inline constexpr std::uint32_t InitialOffsetSetterPC = 0x00540CC0;
inline constexpr std::uint32_t InitialOffsetGetterPC = 0x00558A30;
}
