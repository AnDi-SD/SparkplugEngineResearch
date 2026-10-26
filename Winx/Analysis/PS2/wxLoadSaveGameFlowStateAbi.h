#pragma once

#include "wxGameFlowStateAbi.h"

namespace winx::evidence::ps2
{
    using wxLoadSaveGameFlowStateLayout = wxGameFlowStateLayout;
    static_assert(sizeof(wxLoadSaveGameFlowStateLayout) == 0x3C);
    inline constexpr std::uint32_t wxLoadSaveGameFlowStateFactory = 0x00344460;
    inline constexpr std::uint32_t wxLoadSaveGameFlowStateVTable = 0x004940A0;
    inline constexpr std::uint32_t wxLoadSaveGameFlowStateRTTI = 0x004C44C0;
    inline constexpr std::uint32_t wxLoadSaveGameFlowStateSlot2C = 0x00344270;
    inline constexpr std::uint32_t wxLoadSaveGameFlowStateSlot38 = 0x003441C0;
}
