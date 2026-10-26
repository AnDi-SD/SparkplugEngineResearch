#pragma once
#include "wxGameFlowStateAbi.h"
namespace winx::evidence::pc
{
    using wxDebugMenuGameFlowStateLayout=wxGameFlowStateLayout;
    static_assert(sizeof(wxDebugMenuGameFlowStateLayout)==0x3C);
    inline constexpr std::uint32_t wxDebugMenuGameFlowStateFactory=0x005DD8A0;
    inline constexpr std::uint32_t wxDebugMenuGameFlowStateVTable=0x0070F578;
    inline constexpr std::uint32_t wxDebugMenuGameFlowStateRTTI=0x007684B0;
    inline constexpr std::uint32_t wxDebugMenuGameFlowStateSlot20=0x005DD980;
    inline constexpr std::uint32_t wxDebugMenuGameFlowStateSlot24=0x005DD9D0;
}
