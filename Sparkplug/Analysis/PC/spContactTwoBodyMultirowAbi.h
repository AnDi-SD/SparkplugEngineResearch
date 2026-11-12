#pragma once
#include "spContactRowAbi.h"
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Native method/body offsets only, not a portable native object layout.
    inline constexpr std::uint32_t spContactTwoBodyMultirowOwn20=0x48B610;
    inline constexpr std::size_t spContactTwoBodyMultirowOwn20Bytes=1620;
    inline constexpr std::uint32_t spContactTwoBodyMultirowActualBase=0x48B3C0;
    inline constexpr std::size_t spContactTwoBodyMultirowActualBaseBytes=144;
    inline constexpr std::uint32_t spContactTwoBodyMultirowTangent=0x48B220;
    inline constexpr std::size_t spContactTwoBodyMultirowTangentBytes=183;
    inline constexpr std::size_t spContactTwoBodyMultirowProjectionBytes=480;
    inline constexpr std::size_t spContactTwoBodyMultirowRowStrideWords=8;
    inline constexpr std::uint32_t spContactTwoBodyMultirowManager7C=0x3E4CCCCD;
    inline constexpr std::uint32_t spContactTwoBodyMultirowManager80=0x3727C5AC;
}
