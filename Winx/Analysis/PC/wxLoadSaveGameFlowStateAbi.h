#pragma once

#include "wxGameFlowStateAbi.h"

namespace winx::evidence::pc
{
    // Native construction writes only the derived vtable after the parent.
    using wxLoadSaveGameFlowStateLayout = wxGameFlowStateLayout;
    static_assert(sizeof(wxLoadSaveGameFlowStateLayout) == 0x3C);
    inline constexpr std::uint32_t wxLoadSaveGameFlowStateFactory = 0x005DB060;
    inline constexpr std::uint32_t wxLoadSaveGameFlowStateVTable = 0x0070F314;
    inline constexpr std::uint32_t wxLoadSaveGameFlowStateRTTI = 0x00768390;
    inline constexpr std::uint32_t wxLoadSaveGameFlowStateSlot24 = 0x005DB140;
    inline constexpr std::uint32_t wxLoadSaveGameFlowStateSlot30 = 0x005DB160;
}
