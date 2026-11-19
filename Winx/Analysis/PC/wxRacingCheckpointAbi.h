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

namespace wxRacingCheckpointAbi
{
inline constexpr std::size_t CapturedFinishMinimumPrefixBytes = 0x141;
inline constexpr std::size_t FinishRawByteOffset = 0x140;
inline constexpr std::uint32_t FinishRawByteSetterPC = 0x0057EE80;
inline constexpr std::uint32_t FinishRawByteGetterPC = 0x0057EE90;
// MOV AL preserves native EAX upper24. The value facade exposes only raw bytes.
}
