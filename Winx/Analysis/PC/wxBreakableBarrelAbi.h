#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxBreakableBarrelAbi
{
    inline constexpr std::uint32_t ClassId = 0x05FF35BE;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FC930;
    inline constexpr std::uint32_t OwnPublisherPC = 0x00542620;
    inline constexpr std::size_t CapturedPrefixBytes = 0x18C;
    inline constexpr std::array<std::size_t, 2> RawOffsets{0x180, 0x184};
    inline constexpr std::array<std::uint32_t, 2> RawSettersPC{0x00541C90, 0x00541CA0};
    inline constexpr std::array<std::uint32_t, 2> RawGettersPC{0x00541C80, 0x00539560};
}
