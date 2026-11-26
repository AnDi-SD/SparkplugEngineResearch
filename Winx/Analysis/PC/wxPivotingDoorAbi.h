#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxPivotingDoorClassID = 0x4AC3694A;
    inline constexpr std::uint32_t wxPivotingDoorPrimary = 0x006FBDE0;
    inline constexpr std::uint32_t wxPivotingDoorInteraction = 0x00539C60;
    inline constexpr std::uint32_t wxPivotingDoorCachedDistanceEntry = 0x00539C90;
    inline constexpr std::size_t wxPivotingDoorInteractionSlot = 0x4C;
    inline constexpr std::size_t wxPivotingDoorCaptureBytes = 0x1CC;
    inline constexpr std::size_t wxPivotingDoorInteractionFlag = 0x169;
    inline constexpr std::size_t wxPivotingDoorSquaredThreshold = 0x128;
    inline constexpr std::size_t wxPivotingDoorCachedX = 0x190;
    inline constexpr std::size_t wxPivotingDoorPlayerNodePrefixBytes = 0x80;
    inline constexpr std::size_t wxPivotingDoorPlayerNodeX = 0x74;
    inline constexpr float wxPivotingDoorHeightTolerance = 200.0F;
}
