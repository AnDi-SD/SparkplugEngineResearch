#pragma once
#include <cstddef>
#include <cstdint>

namespace wxFallingRockAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x006FE288;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x18C;
inline constexpr std::size_t WarningDeltaRawOffset = 0x188;
inline constexpr std::size_t FallingDurationRawOffset = 0x184;
inline constexpr std::size_t FallingIntervalRawOffset = 0x180;
inline constexpr std::size_t InflictedDamageRawOffset = 0x170;
inline constexpr std::uint32_t WarningDeltaSetterPC = 0x0056C530;
inline constexpr std::uint32_t WarningDeltaGetterPC = 0x00556E20;
inline constexpr std::uint32_t FallingDurationSetterPC = 0x00541CA0;
inline constexpr std::uint32_t FallingDurationGetterPC = 0x00539560;
inline constexpr std::uint32_t FallingIntervalSetterPC = 0x00541C90;
inline constexpr std::uint32_t FallingIntervalGetterPC = 0x00541C80;
inline constexpr std::uint32_t InflictedDamageSetterPC = 0x005427C0;
inline constexpr std::uint32_t InflictedDamageGetterPC = 0x00556E00;
// Historical factory allocation476B is separate from the touched396B prefix.
// Current constructor body, physical inheritance and native sizeof are unknown.
}
