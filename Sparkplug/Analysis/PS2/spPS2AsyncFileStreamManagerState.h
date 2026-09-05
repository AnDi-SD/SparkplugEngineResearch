#pragma once

// Pure calculations extracted from spPS2AsyncFileStreamManager::Update.
// Platform CD/DVD commands and the apparently dormant queue producer remain
// outside this testable evidence model.

#include <cstdint>

namespace sparkplug::evidence::ps2
{
    inline constexpr std::uint32_t spPS2AsyncRequestCapacity = 50;
    inline constexpr std::uint32_t spPS2AsyncRequestSize = 0x11C;
    inline constexpr std::uint32_t spPS2AsyncWatchdogFrames = 0x14;
    inline constexpr std::uint32_t spPS2DiscSectorBytes = 0x800;

    [[nodiscard]] constexpr std::uint32_t PS2AsyncDestinationCapacity(
        const std::uint32_t byteCount) noexcept
    {
        // Native: (byteCount & -0x800) + 0x1000.  It deliberately adds two
        // sectors even when byteCount is already sector-aligned.
        return (byteCount & 0xFFFFF800U) + 0x1000U;
    }

    [[nodiscard]] constexpr std::uint32_t PS2AsyncDiscSectorCount(
        const std::uint32_t byteCount) noexcept
    {
        // Native: (byteCount >> 11) + 1.  Exact multiples therefore receive
        // one extra sector rather than conventional ceiling division.
        return (byteCount >> 11U) + 1U;
    }
}
