#pragma once
#include "wxAIActionAbi.h"
#include <array>

namespace winx::evidence::pc
{
    // Observed native prefix/tail; encoded pointer words are opaque data.
    struct wxSpiderAttackAIActionLayout final
    { wxAIActionLayout base; std::array<std::uint8_t, 988-0x3A8> rawOwn3A8; };
    static_assert(sizeof(wxSpiderAttackAIActionLayout) == 988);
    static_assert(offsetof(wxSpiderAttackAIActionLayout, rawOwn3A8) == 0x3A8);
    inline constexpr std::uint32_t wxSpiderAttackAIActionClassID = 0x71237CFA;
    inline constexpr Address32 wxSpiderAttackAIActionPrimary = 0x007095F8;
    inline constexpr Address32 wxSpiderAttackAIActionFactory = 0x005B3700;
    inline constexpr Address32 wxSpiderAttackAIActionConstructor = 0x005B34E0;
    inline constexpr Address32 wxSpiderAttackAIActionRTTIGetter = 0x005B33E0;
    inline constexpr Address32 wxSpiderAttackAIActionRTTIRecord = 0x007673D0;
    inline constexpr Address32 wxSpiderAttackAIActionStateGate = 0x005B3470;
    inline constexpr std::size_t wxSpiderAttackAIActionStateGateBytes = 102;
    inline constexpr Address32 wxSpiderAttackAIActionUpdate = 0x005B3CB0;
    inline constexpr std::uint32_t wxSpiderAttackAIActionDispatchPhase = 2;
}
