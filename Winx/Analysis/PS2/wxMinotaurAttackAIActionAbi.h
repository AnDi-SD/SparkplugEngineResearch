#pragma once
#include "Analysis/PS2/wxAIActionAbi.h"

namespace winx::evidence::ps2
{
    struct wxMinotaurAttackAIActionLayout final
    {
        wxAIActionLayout base;
        std::uint32_t borrowedTarget,phase,deadline,randomWord3B8,word3BC;
        std::uint8_t byte3C0,byte3C1,padding3C2[2];
        std::uint32_t borrowedCommand,unwritten3C8;
    };
    static_assert(sizeof(wxMinotaurAttackAIActionLayout)==0x3CC);
    static_assert(offsetof(wxMinotaurAttackAIActionLayout,borrowedCommand)==0x3C4);
    inline constexpr std::uint32_t wxMinotaurAttackAIActionFactory=0x25FA60;
    inline constexpr std::uint32_t wxMinotaurAttackAIActionVtable=0x492BC0; //two zero headers, then17slots
    // Static construction/layout only; no PS2 execution parity is claimed.
}
