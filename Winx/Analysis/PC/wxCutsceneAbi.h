#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// Captured prefix metadata, not a native C++ ABI or object-size declaration.
namespace wxCutsceneAbi
{
inline constexpr std::uint32_t CapturedPrimary = 0x006F8134;
inline constexpr std::size_t CapturedMinimumPrefixBytes = 448;
inline constexpr std::array<std::size_t, 1> ScalarOffsets{0x1BC};
// Original label bytes; the runtime text encoding is unqualified.
inline constexpr std::array<std::uint8_t, 22> PublisherLabelBytes{0x50, 0x72, 0x6F, 0x78, 0x69, 0x6D, 0x69, 0x74, 0x79, 0x20, 0x72, 0x61, 0x64, 0x69, 0x75, 0x73, 0x20, 0x28, 0x63, 0x6D, 0xB2, 0x29};
}
