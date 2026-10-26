#pragma once
#include "Analysis/PC/wxGameFlowStateAbi.h"

namespace winx::evidence::ps2
{
    // The observed offsets coincide. This is not a claim of PC constructor
    // parity: PS2 initializes codes from three current globals after defaults.
    using wxGameFlowStateLayout = winx::evidence::pc::wxGameFlowStateLayout;
    inline constexpr std::uint32_t wxGameFlowStateConstructor = 0x0033C370;
    inline constexpr std::uint32_t wxGameFlowStateVTable = 0x00493F10;
    inline constexpr std::uint32_t wxGameFlowStateSetCodes = 0x0033C230;
    inline constexpr std::uint32_t wxGameFlowStateGetCodes = 0x0033C260;
    inline constexpr std::uint32_t wxGameFlowStateRemapResource = 0x0033B430;
}
