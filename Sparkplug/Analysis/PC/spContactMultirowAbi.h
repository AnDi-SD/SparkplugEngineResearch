#pragma once
#include "spContactRowAbi.h"
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Native method/body offsets only, not a portable native object layout.
    inline constexpr std::uint32_t spContactMultirowOwn20=0x48B610;
    inline constexpr std::size_t spContactMultirowOwn20Bytes=1620;
    inline constexpr std::uint32_t spContactMultirowActualBase=0x48B3C0;
    inline constexpr std::size_t spContactMultirowActualBaseBytes=144;
    inline constexpr std::uint32_t spContactMultirowTangent=0x48B220;
    inline constexpr std::size_t spContactMultirowTangentBytes=183;
    inline constexpr std::size_t spContactMultirowProjectionBytes=384;
    inline constexpr std::size_t spContactMultirowRowStrideWords=8;
    inline constexpr std::uint32_t spContactMultirowManager7C=0x3E4CCCCD;
    inline constexpr std::uint32_t spContactMultirowManager80=0x3727C5AC;
}
