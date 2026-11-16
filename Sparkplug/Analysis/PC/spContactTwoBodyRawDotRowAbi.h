#pragma once
#include "spContactTwoBodyRowAbi.h"
#include <array>
namespace sparkplug::evidence::pc
{
    inline constexpr std::uint32_t spContactTwoBodyRawDotOriginalMethod=0x0048B610U;
    inline constexpr std::uint32_t spContactTwoBodyRawDotFinalAddPC=0x0048B8ACU;
    inline constexpr std::uint32_t spContactTwoBodyRawDotOptionalStorePC=0x0048B8E6U;
    inline constexpr std::size_t spContactTwoBodyRawDotMultiplier94=0x94;
    inline constexpr std::size_t spContactTwoBodyRawDotThreshold98=0x98;
    inline constexpr std::array<std::size_t,6> spContactTwoBodyRawDotBodyReadOffsets{0x8C,0x90,0x94,0x98,0x9C,0xA0};
    // Observed raw PC roles only, not a native class overlay or pointer ABI.
}
