#pragma once
#include "Analysis/PC/wxAIActionAbi.h"

namespace winx::evidence::pc
{
    struct wxMinotaurAttackAIActionLayout final
    {
        wxAIActionLayout base;
        std::uint32_t borrowedTarget,phase,deadline,randomWord3B4,word3B8;
        std::uint8_t byte3BC,byte3BD,padding3BE[2];
        std::uint32_t borrowedCommand,unwritten3C4;
    };
    static_assert(sizeof(wxMinotaurAttackAIActionLayout)==0x3C8);
    static_assert(offsetof(wxMinotaurAttackAIActionLayout,borrowedCommand)==0x3C0);
    inline constexpr std::uint32_t wxMinotaurAttackAIActionFactory=0x5B8E80;
    inline constexpr std::uint32_t wxMinotaurAttackAIActionVtable=0x709AC8;
    inline constexpr std::uint32_t wxMinotaurAttackAIActionEntry=0x5B8F50;
    inline constexpr std::uint32_t wxMinotaurAttackAIActionNotify=0x5B8FF0;
    inline constexpr std::uint32_t wxMinotaurAttackAIActionUpdate=0x5B9760;
    inline constexpr std::uint32_t wxMinotaurAttackAIActionCooldown=0x5B9130; //phase3, literal jump table
    inline constexpr std::uint32_t wxMinotaurAttackAIActionMovementPhase2=0x5B91D0;
}
