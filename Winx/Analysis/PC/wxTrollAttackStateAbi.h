#pragma once

#include "wxAnimationControllerAbi.h"
#include "wxCharacterStateAbi.h"

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxTrollAttackStateClassID = 0x03D479EB;
    inline constexpr std::uint32_t wxTrollAttackStateVTable = 0x006F9120;
    inline constexpr std::uint32_t wxTrollAttackStateCompletionMethod = 0x0051A290;
    inline constexpr std::uint32_t wxTrollAttackStateConsumeMethod = 0x004FB330;
    inline constexpr std::size_t wxTrollAttackStateCompletionSlotOffset = 0x34;
    inline constexpr std::size_t wxTrollAttackStateConsumerOffset = offsetof(wxCharacterStateLayout, completionConsumer);
    inline constexpr std::size_t wxTrollAttackStatePendingOffset = offsetof(wxCharacterStateLayout, pendingHandle);
    inline constexpr std::size_t wxTrollAttackStateMark3Offset = offsetof(wxAnimationControllerLayout, mark3);
    inline constexpr std::size_t wxTrollAttackStateMark9Offset = offsetof(wxAnimationControllerLayout, mark9);

    static_assert(wxTrollAttackStateConsumerOffset == 0x18);
    static_assert(wxTrollAttackStatePendingOffset == 0x24);
    static_assert(wxTrollAttackStateMark9Offset == 0x170);
    static_assert(wxTrollAttackStateMark3Offset == 0x174);
}
