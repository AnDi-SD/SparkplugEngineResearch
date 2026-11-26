#pragma once

#include "wxCharacterStateAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxGolemAttackStateClassID = 0x1B610315;
    inline constexpr std::uint32_t wxGolemAttackStateVTable = 0x006F9220;
    inline constexpr std::uint32_t wxGolemAttackStateRecordClearMethod = 0x00523690;
    inline constexpr std::size_t wxGolemAttackStateRecordClearSlotOffset = 0x30;
    inline constexpr std::size_t wxGolemAttackStateCharacterLinkOffset = offsetof(wxCharacterStateLayout, owner);
    inline constexpr std::size_t wxGolemAttackStateCharacterRecordLinkOffset = 0x12C;
    inline constexpr std::size_t wxGolemAttackStateRecordClearWordOffset = 0x04;

    static_assert(wxGolemAttackStateCharacterLinkOffset == 0x14);
}
