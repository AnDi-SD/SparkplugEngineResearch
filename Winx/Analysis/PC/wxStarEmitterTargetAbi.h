#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wxStarEmitterTargetAbi
{
    inline constexpr std::uint32_t ClassId = 0xC26435BA;
    inline constexpr std::uint32_t CapturedPrimary = 0x006F8088;
    inline constexpr std::uint32_t OwnPublisherPC = 0x00512520;
    inline constexpr std::size_t CapturedMinimumPrefixBytes = 0x15C;
    inline constexpr std::array<std::size_t, 2> RawOffsets{0x154, 0x158};
    inline constexpr std::array<std::uint32_t, 2> RawSettersPC{0x0054FDC0, 0x00540D20};
    inline constexpr std::array<std::uint32_t, 2> RawGettersPC{0x0053D050, 0x00540D30};
}
