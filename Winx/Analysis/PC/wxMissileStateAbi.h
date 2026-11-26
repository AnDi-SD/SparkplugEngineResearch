#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxMissileStateClassID = 0x29B0FF3A;
    inline constexpr std::uint32_t wxMissileStatePrimary = 0x006F8BE0;
    inline constexpr std::uint32_t wxMissileStatePermissionByte = 0x00518780;
    inline constexpr std::size_t wxMissileStatePermissionSlotOffset = 0x34;
    inline constexpr std::size_t wxMissileStatePermissionByteOffset = 0x3C;
    inline constexpr std::size_t wxMissileStateCaptureBytes = 0x40;
}
