#pragma once

#include "wxCharacterStateAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxChargedAttackStateClassID = 0x06C1726B;
    inline constexpr std::uint32_t wxChargedAttackStateVTable = 0x006F83F8;
    inline constexpr std::uint32_t wxChargedAttackStatePermission = 0x00514720;
    inline constexpr std::size_t wxChargedAttackStatePermissionSlotOffset = 0x34;
    inline constexpr std::size_t wxChargedAttackStateOwnerOffset = offsetof(wxCharacterStateLayout, owner);
    inline constexpr std::size_t wxChargedAttackStateCharacterWordOffset = 0x140;

    static_assert(wxChargedAttackStateOwnerOffset == 0x14);
}
