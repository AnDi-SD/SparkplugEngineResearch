#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxIceBatStateMachineAbi
{
    inline constexpr std::uint32_t ClassId = 0x448212BB;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FAA58;
    inline constexpr std::uint32_t OwnPublisherPC = 0x0052ACF0;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x234;
    inline constexpr std::array<std::size_t, 2> ScalarOffsets{0x22C, 0x230};
    inline constexpr std::array<std::uint32_t, 2> ScalarGettersPC{0x0052A770, 0x005373A0};
    inline constexpr std::array<std::uint32_t, 2> ScalarSettersPC{0x0052A780, 0x00532B20};
}
