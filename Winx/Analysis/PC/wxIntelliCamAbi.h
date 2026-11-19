#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// Captured prefix metadata, not a native C++ ABI or object-size declaration.
namespace wxIntelliCamAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x006F522C;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 1000;
inline constexpr std::array<std::size_t, 10> ScalarOffsets{0x3E4, 0x3E0, 0x18C, 0x1A8, 0x170, 0x188, 0x1A4, 0x184, 0x1A0, 0x14C};
}
