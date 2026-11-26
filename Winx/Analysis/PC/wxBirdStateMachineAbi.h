#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxBirdStateMachineAbi
{
    inline constexpr std::uint32_t ClassId = 0x3CE04BD0;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FA878;
    inline constexpr std::uint32_t OwnPublisherPC = 0x00529EA0;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x264;
    inline constexpr std::array<std::size_t, 2> RadiusOffsets{0x25C, 0x260};
    inline constexpr std::array<std::uint32_t, 2> RadiusGettersPC{0x00529010, 0x00529020};
    inline constexpr std::array<std::uint32_t, 2> RadiusSettersPC{0x0052E370, 0x00529030};
}
