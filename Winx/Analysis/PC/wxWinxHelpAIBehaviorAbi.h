#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxWinxHelpAIBehaviorAbi
{
    inline constexpr std::uint32_t ClassId = 0x423845B1;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FFEE0;
    inline constexpr std::uint32_t OwnPublisherPC = 0x00569C10;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x1D0;
    inline constexpr std::array<std::size_t, 3> RawOffsets{0x1C4, 0x1C8, 0x1CC};
    inline constexpr std::array<std::uint32_t, 3> RawSettersPC{0x005640A0, 0x00562300, 0x00562310};
    inline constexpr std::array<std::uint32_t, 3> RawGettersPC{0x00554170, 0x00554180, 0x00554190};
}
