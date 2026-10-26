#pragma once
#include "Analysis/PS2/wxAIActionAbi.h"
namespace winx::evidence::ps2
{
    struct wxTrollBeforeFightAIActionLayout final
    { wxAIActionLayout base; std::uint32_t phase, latch, nextDecision, cooldown; };
    static_assert(sizeof(wxTrollBeforeFightAIActionLayout) == 0x3BC);
    static_assert(offsetof(wxTrollBeforeFightAIActionLayout, cooldown) == 0x3B8);
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionFactory = 0x26B8B0;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionVtable = 0x492E60;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionTick = 0x26B6C0;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionEntry = 0x26B620;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionExit = 0x26B5B0;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionCopy = 0x26B4F0;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionStage0 = 0x26B360;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionStage1 = 0x26B300;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionStage2 = 0x26B260;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionStage3 = 0x26B200;
    inline constexpr std::uint32_t wxTrollBeforeFightAIActionQuery = 0x26B640;
}
