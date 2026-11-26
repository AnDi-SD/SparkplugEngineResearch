#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxSpinningStatuesPuzzleClassID = 0x0419573B;
    inline constexpr std::uint32_t wxSpinningStatuesPuzzleVTable = 0x00700A08;
    inline constexpr std::uint32_t wxSpinningStatuesPuzzleCopyMethod = 0x00578D80;
    inline constexpr std::uint32_t wxSpinningStatuesPuzzleRttiMethod = 0x00578D70;
    inline constexpr std::uint32_t wxSpinningStatuesPuzzleCopyPayloadEntry = 0x00578D97;
    inline constexpr std::uint32_t wxSpinningStatuesPuzzleCopyPayloadStop = 0x00578E27;
    inline constexpr std::size_t wxSpinningStatuesPuzzleCopySlotOffset = 0x0C;
    inline constexpr std::size_t wxSpinningStatuesPuzzleCaptureBytes = 0x15C;
}
