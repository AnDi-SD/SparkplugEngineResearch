#pragma once
#include "Analysis/PS2/wxAIActionAbi.h"

namespace winx::evidence::ps2
{
    struct wxMinotaurDefendAIActionLayout final
    {
        wxAIActionLayout base;
        std::uint32_t borrowedTarget, state, deadline, borrowedCommand;
    };
    static_assert(sizeof(wxMinotaurDefendAIActionLayout) == 0x3BC);
    static_assert(offsetof(wxMinotaurDefendAIActionLayout, borrowedCommand) == 0x3B8);
    inline constexpr std::uint32_t wxMinotaurDefendAIActionFactory = 0x260150;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionVtable = 0x492C10;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionEntry = 0x25FCE0;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionNotify = 0x25FBF0;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionCopy = 0x25FB50;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionGate = 0x25FDB0;
}
