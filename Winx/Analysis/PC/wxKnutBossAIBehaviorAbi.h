#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxKnutBossAIBehaviorAbi
{
    inline constexpr std::uint32_t ClassId = 0x0D367CE3;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FF490;
    inline constexpr std::uint32_t OwnPublisherPC = 0x00562A40;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x1D0;
    inline constexpr std::array<std::size_t, 4> RawOffsets{0x1B0, 0x1B4, 0x1C8, 0x1CC};
    inline constexpr std::array<std::uint32_t, 4> RawSettersPC{0x00563230, 0x00563240, 0x00562300, 0x00562310};
    inline constexpr std::array<std::uint32_t, 4> RawGettersPC{0x0054ED20, 0x00561020, 0x00554180, 0x00554190};
}
