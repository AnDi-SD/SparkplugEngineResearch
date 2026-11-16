#pragma once
#include "spContactRowAbi.h"
#include <cstddef>
#include <cstdint>
namespace sparkplug::evidence::pc
{
    inline constexpr std::uint32_t spContactRawDotRowOriginalMethod = 0x0048B610U;
    inline constexpr std::uint32_t spContactRawDotRowOptionalStorePC = 0x0048B8E6U;
    inline constexpr std::size_t spContactRawDotRowMultiplier94 = 0x94;
    inline constexpr std::size_t spContactRawDotRowThreshold98 = 0x98;
    inline constexpr std::array<std::size_t,6> spContactRawDotBodyReadOffsets
        {0x8C,0x90,0x94,0x98,0x9C,0xA0};
    // Raw PC data roles only. No portable/native pointer layout, physical
    // base or recovered semantic names are declared by this analysis header.
}
