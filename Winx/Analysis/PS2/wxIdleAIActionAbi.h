#pragma once
#include "Analysis/PS2/wxAIActionAbi.h"

namespace winx::evidence::ps2
{
    using wxIdleAIActionLayout = wxAIActionLayout;
    inline constexpr std::uint32_t wxIdleAIActionFactory = 0x25B0C0;
    inline constexpr std::uint32_t wxIdleAIActionVtable = 0x492A30;
    inline constexpr std::uint32_t wxIdleAIActionClone = 0x25AFF0;
    inline constexpr std::uint32_t wxIdleAIActionCopy = 0x25AE70;
    inline constexpr std::uint32_t wxIdleAIActionEntry = 0x25AF10;
    inline constexpr std::uint32_t wxIdleAIActionQuery = 0x25AE00;
}
