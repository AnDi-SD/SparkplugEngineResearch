#pragma once
#include "wxGameFlowStateAbi.h"
namespace winx::evidence::ps2
{
    using wxDialogWindowGameFlowStateLayout=wxGameFlowStateLayout;
    static_assert(sizeof(wxDialogWindowGameFlowStateLayout)==0x3C);
    inline constexpr std::uint32_t wxDialogWindowGameFlowStateFactory=0x00327F70;
    inline constexpr std::uint32_t wxDialogWindowGameFlowStateVTable=0x00493DA0;
    inline constexpr std::uint32_t wxDialogWindowGameFlowStateRTTI=0x004C3E60;
    inline constexpr std::uint32_t wxDialogWindowGameFlowStateSlot2C=0x00327DE0;
    inline constexpr std::uint32_t wxDialogWindowGameFlowStateSlot38=0x00327D80;
}
