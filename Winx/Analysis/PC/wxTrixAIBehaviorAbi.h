#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxTrixAIBehaviorAbi
{
    inline constexpr std::uint32_t ClassId = 0x426D268C;
    inline constexpr std::uint32_t CapturedPrimary = 0x007003D0;
    inline constexpr std::uint32_t OwnPublisherPC = 0x005704D0;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x1C8;
    inline constexpr std::array<std::size_t, 4> RawOffsets{0x1B8, 0x1BC, 0x1C0, 0x1C4};
    inline constexpr std::array<std::uint32_t, 4> RawSettersPC{0x00563250, 0x00564080, 0x00564090, 0x005640A0};
    inline constexpr std::array<std::uint32_t, 4> RawGettersPC{0x0054F0B0, 0x00561030, 0x005541C0, 0x00554170};
}
