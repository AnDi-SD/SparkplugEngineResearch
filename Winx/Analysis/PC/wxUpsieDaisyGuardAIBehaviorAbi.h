#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace wxUpsieDaisyGuardAIBehaviorAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x007001E0;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x188;
inline constexpr std::size_t GuardNumRawOffset = 0x184;
inline constexpr std::uint32_t GuardNumSetterPC = 0x00541CA0;
inline constexpr std::uint32_t GuardNumGetterPC = 0x00539560;
inline constexpr std::uint32_t ClassId = 0x0A71019C;
inline constexpr std::uint32_t OwnPublisherPC = 0x0056D3F0;
inline constexpr std::size_t ScalarMinimumPrefixBytes = 0x19C;
inline constexpr std::array<std::size_t, 4> ScalarOffsets{0x164, 0x188, 0x194, 0x198};
inline constexpr std::array<std::uint32_t, 4> ScalarSettersPC{0x00567EB0, 0x0056C530, 0x0056C540, 0x0056C550};
inline constexpr std::array<std::uint32_t, 4> ScalarGettersPC{0x00555EC0, 0x004DAE40, 0x004FC880, 0x004FC890};
inline constexpr std::size_t ByteMinimumPrefixBytes = 0x1D6;
inline constexpr std::array<std::size_t, 2> ByteOffsets{0x1D4, 0x1D5};
inline constexpr std::array<std::uint32_t, 2> ByteSettersPC{0x0056C570, 0x0056C590};
inline constexpr std::array<std::uint32_t, 2> ByteGettersPC{0x0056C560, 0x0056C580};
// Historical allocation 488 B is separate from the 392 B, 412 B and 470 B selected prefixes.
// Current constructor body, native sizeof and physical inheritance remain unknown.
}
