#pragma once
#include "Analysis/PC/wxAIActionAbi.h"

namespace winx::evidence::pc
{
    // Idle has the exact base allocation and no added native fields.
    using wxIdleAIActionLayout = wxAIActionLayout;
    inline constexpr std::uint32_t wxIdleAIActionFactory = 0x5D66D0;
    inline constexpr std::uint32_t wxIdleAIActionVtable = 0x70EEC8;
    inline constexpr std::uint32_t wxIdleAIActionClone = 0x5D6740;
    inline constexpr std::uint32_t wxIdleAIActionCopy = 0x5D67B0;
    inline constexpr std::uint32_t wxIdleAIActionEntry = 0x5D66A0;
    inline constexpr std::uint32_t wxIdleAIActionQuery = 0x5B5100;
}
