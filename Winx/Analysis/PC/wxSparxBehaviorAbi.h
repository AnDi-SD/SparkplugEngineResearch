#pragma once
#include <cstddef>
#include <cstdint>

namespace wxSparxBehaviorAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x006F6F60;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 297;
inline constexpr std::size_t TestAnimationRawByteOffset = 0x128;
inline constexpr std::uint32_t TestAnimationSetterPC = 0x00505800;
inline constexpr std::uint32_t TestAnimationGetterPC = 0x00505810;
// SET reads one byte from argument slot +4, writes AL, and RET 4.
// GET reads one receiver byte into AL. The facade exposes only that raw byte.
}
