#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxGhostlySpiritStateMachineAbi
{
    inline constexpr std::uint32_t ClassId = 0x88760331;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FAAD8;
    inline constexpr std::uint32_t OwnPublisherPC = 0x0052B3D0;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x240;
    inline constexpr std::array<std::size_t, 4> ScalarOffsets{0x230, 0x234, 0x238, 0x23C};
    inline constexpr std::array<std::uint32_t, 4> ScalarGettersPC{0x005373A0, 0x004FD7C0, 0x004FD7D0, 0x0052AEB0};
    inline constexpr std::array<std::uint32_t, 4> ScalarSettersPC{0x00532B20, 0x0052AE90, 0x0052AEA0, 0x0052AEC0};
}
