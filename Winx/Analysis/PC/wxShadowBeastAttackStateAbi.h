#pragma once

#include "wxCharacterStateAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxShadowBeastAttackStateClassID = 0x0B6B139A;
    inline constexpr std::uint32_t wxShadowBeastAttackStateVTable = 0x006F9E00;
    inline constexpr std::uint32_t wxShadowBeastAttackStatePermissionMethod = 0x00522990;
    inline constexpr std::uint32_t wxShadowBeastAttackStateConsumeMethod = 0x004FB330;
    inline constexpr std::size_t wxShadowBeastAttackStatePermissionSlotOffset = 0x34;
    inline constexpr std::size_t wxShadowBeastAttackStateCaptureSize = 0x58;
    inline constexpr std::size_t wxShadowBeastAttackStateConsumerOffset = offsetof(wxCharacterStateLayout, completionConsumer);
    inline constexpr std::size_t wxShadowBeastAttackStatePendingOffset = offsetof(wxCharacterStateLayout, pendingHandle);

    static_assert(wxShadowBeastAttackStateConsumerOffset == 0x18);
    static_assert(wxShadowBeastAttackStatePendingOffset == 0x24);
    static_assert(wxShadowBeastAttackStatePendingOffset + sizeof(std::uint32_t) <= wxShadowBeastAttackStateCaptureSize);
}
