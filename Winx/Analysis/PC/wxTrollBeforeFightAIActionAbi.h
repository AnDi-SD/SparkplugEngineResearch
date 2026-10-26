#pragma once
#include "Analysis/PC/wxAIActionAbi.h"
namespace winx::evidence::pc
{
    struct wxTrollBeforeFightAIActionLayout final
    { wxAIActionLayout base; std::uint32_t phase, latch, nextDecision, cooldown; };
    static_assert(sizeof(wxTrollBeforeFightAIActionLayout) == 0x3B8);
    static_assert(offsetof(wxTrollBeforeFightAIActionLayout, cooldown) == 0x3B4);
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionFactory = 0x5B7650;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionVtable = 0x709900;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionTick = 0x5B74D0;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionEntry = 0x5B7520;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionExit = 0x5B7540;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionCopy = 0x5B7740;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionStage0 = 0x5B77C0;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionStage1 = 0x5B75B0;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionStage2 = 0x5B78C0;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionStage3 = 0x5B7600;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionQuery = 0x5B79A0;
}
