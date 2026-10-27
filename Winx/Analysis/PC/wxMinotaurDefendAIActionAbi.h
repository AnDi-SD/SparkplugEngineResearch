#pragma once
#include "Analysis/PC/wxAIActionAbi.h"

namespace winx::evidence::pc
{
    struct wxMinotaurDefendAIActionLayout final
    {
        wxAIActionLayout base;
        std::uint32_t borrowedTarget, state, deadline, borrowedCommand;
    };
    static_assert(sizeof(wxMinotaurDefendAIActionLayout) == 0x3B8);
    static_assert(offsetof(wxMinotaurDefendAIActionLayout, borrowedCommand) == 0x3B4);
    inline constexpr std::uint32_t wxMinotaurDefendAIActionFactory = 0x5B8940;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionVtable = 0x709A50;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionEntry = 0x5B8A40;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionNotify = 0x5B8AD0;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionCopy = 0x5B8B30;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionGate = 0x5B8D80;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionFacing = 0x5B8BB0;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionState0 = 0x5B8C30;
    inline constexpr std::uint32_t wxMinotaurDefendAIActionState1 = 0x5B8CE0;
}
