#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxTargetRegionClassID = 0x4307193A;
    inline constexpr std::uint32_t wxTargetRegionVTable = 0x006F7ED0;
    inline constexpr std::uint32_t wxTargetRegionCopyMethod = 0x00511160;
    inline constexpr std::uint32_t wxTargetRegionCopyPayloadEntry = 0x00511177;
    inline constexpr std::uint32_t wxTargetRegionCopyPayloadStop = 0x005111E3;
    inline constexpr std::size_t wxTargetRegionCopySlotOffset = 0x0C;
    inline constexpr std::size_t wxTargetRegionTextOffset = 0x124;
    inline constexpr std::size_t wxTargetRegionTextCaptureExtent = 32;
}
