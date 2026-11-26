#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxIceWormAIBehaviorAbi
{
    inline constexpr std::uint32_t ClassId = 0x169B1DA0;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FF630;
    inline constexpr std::uint32_t OwnPublisherPC = 0x005635D0;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x1BC;
    inline constexpr std::array<std::size_t, 2> RawOffsets{0x1B4, 0x1B8};
    inline constexpr std::array<std::uint32_t, 2> RawSettersPC{0x00563240, 0x00563250};
    inline constexpr std::array<std::uint32_t, 2> RawGettersPC{0x00561020, 0x0054F0B0};
}
