#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxDroidAttackStateClassID = 0x4EAE224C;
    inline constexpr std::uint32_t wxDroidAttackStateVTable = 0x006FA020;
    inline constexpr std::uint32_t wxDroidAttackStatePermission = 0x005238D0;
    inline constexpr std::size_t wxDroidAttackStatePermissionSlotOffset = 0x34;
    inline constexpr std::size_t wxDroidAttackStatePermissionByteOffset = 0x3C;
}
