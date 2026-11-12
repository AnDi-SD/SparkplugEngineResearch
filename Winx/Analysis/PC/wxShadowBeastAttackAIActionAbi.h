#pragma once
#include "wxAIActionAbi.h"
#include <array>

namespace winx::evidence::pc
{
    // Observed native prefix/tail; encoded pointer words are opaque data.
    struct wxShadowBeastAttackAIActionLayout final
    { wxAIActionLayout base; std::array<std::uint8_t, 992-0x3A8> rawOwn3A8; };
    static_assert(sizeof(wxShadowBeastAttackAIActionLayout) == 992);
    static_assert(offsetof(wxShadowBeastAttackAIActionLayout, rawOwn3A8) == 0x3A8);
    inline constexpr std::uint32_t wxShadowBeastAttackAIActionClassID = 0x0F6C5372;
    inline constexpr Address32 wxShadowBeastAttackAIActionPrimary = 0x00709B60;
    inline constexpr Address32 wxShadowBeastAttackAIActionFactory = 0x005B9AB0;
    inline constexpr Address32 wxShadowBeastAttackAIActionConstructor = 0x005B9900;
    inline constexpr Address32 wxShadowBeastAttackAIActionRTTIGetter = 0x005B97E0;
    inline constexpr Address32 wxShadowBeastAttackAIActionRTTIRecord = 0x00767790;
    inline constexpr Address32 wxShadowBeastAttackAIActionStateGate = 0x005B9890;
    inline constexpr std::size_t wxShadowBeastAttackAIActionStateGateBytes = 100;
    inline constexpr Address32 wxShadowBeastAttackAIActionUpdate = 0x005BA600;
    inline constexpr std::uint32_t wxShadowBeastAttackAIActionDispatchPhase = 3;
}
