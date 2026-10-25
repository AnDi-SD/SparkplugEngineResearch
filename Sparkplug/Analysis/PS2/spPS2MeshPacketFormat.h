#pragma once

#include <cstdint>

namespace sparkplug::evidence::ps2
{
    // Original1604B0 reads only the descriptor's low16 bits. These are the
    // five observed packet format widths, not a complete VIF ISA decoder.
    // Unknown formats contribute0 to that reservation accumulator.
    [[nodiscard]] constexpr std::uint32_t MeshPacketComponentByteWidthForAnalysis(
        std::uint32_t descriptor) noexcept
    {
        switch (descriptor & 0xFFFFU)
        {
        case 0x64: return 8;
        case 0x68: return 12;
        case 0x6C: return 16;
        case 0x6F: return 2;
        case 0x6E: return 4;
        default: return 0;
        }
    }
}
