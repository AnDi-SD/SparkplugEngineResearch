#pragma once
#include "wxGameFlowStateAbi.h"
namespace winx::evidence::ps2
{
    using wxDebugMenuGameFlowStateLayout=wxGameFlowStateLayout;
    static_assert(sizeof(wxDebugMenuGameFlowStateLayout)==0x3C);
    inline constexpr std::uint32_t wxDebugMenuGameFlowStateFactory=0x00326BC0;
    inline constexpr std::uint32_t wxDebugMenuGameFlowStateVTable=0x00493D10;
    inline constexpr std::uint32_t wxDebugMenuGameFlowStateRTTI=0x004C3DA0;
    inline constexpr std::uint32_t wxDebugMenuGameFlowStateSlot28=0x00326A20;
    inline constexpr std::uint32_t wxDebugMenuGameFlowStateSlot2C=0x003269D0;
}
