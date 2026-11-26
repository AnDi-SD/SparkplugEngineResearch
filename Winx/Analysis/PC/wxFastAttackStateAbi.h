#pragma once

#include "wxCharacterStateAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxFastAttackStateClassID = 0x74F073D7;
    inline constexpr std::uint32_t wxFastAttackStateVTable = 0x006F83A8;
    inline constexpr std::uint32_t wxFastAttackStatePermission = 0x005144C0;
    inline constexpr std::size_t wxFastAttackStatePermissionSlotOffset = 0x34;
    inline constexpr std::size_t wxFastAttackStateOwnerOffset = offsetof(wxCharacterStateLayout, owner);
    inline constexpr std::size_t wxFastAttackStateLeftOffset = 0x3C;
    inline constexpr std::size_t wxFastAttackStateRightOffset = 0x40;
    inline constexpr std::size_t wxFastAttackCharacterWordOffset = 0x140;

    static_assert(wxFastAttackStateOwnerOffset == 0x14);
}
