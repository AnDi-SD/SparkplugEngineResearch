#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxYetiAIBehaviorAbi
{
    inline constexpr std::uint32_t ClassId = 0x26277D61;
    inline constexpr std::uint32_t CapturedPrimary = 0x006FF790;
    inline constexpr std::uint32_t OwnPublisherPC = 0x005643A0;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x1C0;
    inline constexpr std::array<std::size_t, 2> RawOffsets{0x1B8, 0x1BC};
    inline constexpr std::array<std::uint32_t, 2> RawSettersPC{0x00563250, 0x00564080};
    inline constexpr std::array<std::uint32_t, 2> RawGettersPC{0x0054F0B0, 0x00561030};
}
