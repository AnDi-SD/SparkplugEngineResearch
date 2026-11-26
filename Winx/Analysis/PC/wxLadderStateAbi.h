#pragma once

#include "wxCharacterStateAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxLadderStateClassID = 0x298847B5;
    inline constexpr std::uint32_t wxLadderStateVTable = 0x006F8608;
    inline constexpr std::uint32_t wxLadderStatePermission = 0x005161A0;
    inline constexpr std::size_t wxLadderStatePermissionSlotOffset = 0x34;
    inline constexpr std::size_t wxLadderStateCharacterLinkOffset = offsetof(wxCharacterStateLayout, owner);
    inline constexpr std::size_t wxLadderCharacterRecordLinkOffset = 0x12C;
    inline constexpr std::size_t wxLadderRecordResetByteOffset = 0x61;

    static_assert(wxLadderStateCharacterLinkOffset == 0x14);
}
