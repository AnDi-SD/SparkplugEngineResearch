#pragma once
#include <cstddef>
#include <cstdint>

namespace wxRacingCheckpointAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x00701188;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x13C;
inline constexpr std::size_t BonusTimeRawOffset = 0x138;
inline constexpr std::size_t CheckpointIdRawOffset = 0x128;
inline constexpr std::uint32_t BonusTimeSetterPC = 0x005100F0;
inline constexpr std::uint32_t BonusTimeGetterPC = 0x0057FC10;
inline constexpr std::uint32_t CheckpointIdSetterPC = 0x00555060;
inline constexpr std::uint32_t CheckpointIdGetterPC = 0x00555070;
// Historical factory allocation was356B. Native sizeof/current constructor remain unknown.
}
