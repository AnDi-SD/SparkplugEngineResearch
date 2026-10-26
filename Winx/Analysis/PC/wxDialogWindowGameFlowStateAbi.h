#pragma once
#include "wxGameFlowStateAbi.h"
namespace winx::evidence::pc
{
    using wxDialogWindowGameFlowStateLayout=wxGameFlowStateLayout;
    static_assert(sizeof(wxDialogWindowGameFlowStateLayout)==0x3C);
    inline constexpr std::uint32_t wxDialogWindowGameFlowStateFactory=0x005DB1B0;
    inline constexpr std::uint32_t wxDialogWindowGameFlowStateVTable=0x0070F360;
    inline constexpr std::uint32_t wxDialogWindowGameFlowStateRTTI=0x007683F0;
    inline constexpr std::uint32_t wxDialogWindowGameFlowStateSlot24=0x005DB290;
    inline constexpr std::uint32_t wxDialogWindowGameFlowStateSlot30=0x005DB2C0;
}
