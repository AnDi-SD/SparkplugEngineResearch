#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxDoorPuzzleClassID = 0x8719C992;
    inline constexpr std::uint32_t wxDoorPuzzleVTable = 0x0070095C;
    inline constexpr std::uint32_t wxDoorPuzzleCopyMethod = 0x00578270;
    inline constexpr std::uint32_t wxDoorPuzzleRttiMethod = 0x00578260;
    inline constexpr std::uint32_t wxDoorPuzzleCopyPayloadEntry = 0x00578287;
    inline constexpr std::uint32_t wxDoorPuzzleCopyPayloadStop = 0x0057833B;
    inline constexpr std::size_t wxDoorPuzzleCopySlotOffset = 0x0C;
    inline constexpr std::size_t wxDoorPuzzleCaptureBytes = 0x188;
}
