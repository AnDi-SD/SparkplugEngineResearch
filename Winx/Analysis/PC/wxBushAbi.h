#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxBushClassID = 0x0598568B;
    inline constexpr std::uint32_t wxBushVTable = 0x006FCEC8;
    inline constexpr std::uint32_t wxBushCopyMethod = 0x005441A0;
    inline constexpr std::uint32_t wxBushCopyPayloadEntry = 0x005441B7;
    inline constexpr std::uint32_t wxBushCopyPayloadStop = 0x005441EA;
    inline constexpr std::size_t wxBushCopySlotOffset = 0x0C;
    inline constexpr std::size_t wxBushFirstTextOffset = 0x14C;
    inline constexpr std::size_t wxBushSecondTextOffset = 0x16A;
    inline constexpr std::size_t wxBushTextCaptureExtent = 30;
}
