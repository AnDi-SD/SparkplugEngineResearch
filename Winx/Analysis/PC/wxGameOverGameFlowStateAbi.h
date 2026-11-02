#pragma once
#include "wxMenuGameFlowStateAbi.h"

namespace winx::evidence::pc
{
    struct wxGameOverGameFlowStateLayout final
    {
        wxMenuGameFlowStateLayout base;
        std::uint32_t borrowedInput5C;
    };
    static_assert(sizeof(wxGameOverGameFlowStateLayout)==0x60);
    static_assert(offsetof(wxGameOverGameFlowStateLayout,borrowedInput5C)==0x5C);
    inline constexpr std::uint32_t wxGameOverGameFlowStateFactory=0x005E0660;
    inline constexpr std::uint32_t wxGameOverGameFlowStateConstructor=0x005E0460;
    inline constexpr std::uint32_t wxGameOverGameFlowStateDestructor=0x005E0440;
    inline constexpr std::uint32_t wxGameOverGameFlowStateDeletingDestructor=0x005E04D0;
    inline constexpr std::uint32_t wxGameOverGameFlowStateVTable=0x0070F778;
    inline constexpr std::uint32_t wxGameOverGameFlowStateGetter=0x005E0450;
    inline constexpr std::uint32_t wxGameOverGameFlowStateRTTI=0x00768630;
    inline constexpr std::uint32_t wxGameOverGameFlowStateInheritedExit=0x005F14B0;
    inline constexpr std::uint32_t wxGameOverGameFlowStateExitCell=0x0070F79C;
    inline constexpr std::uint32_t wxGameOverGameFlowStateInheritedSlot28=0x005F16D0;
    inline constexpr std::uint32_t wxGameOverGameFlowStateSlot28Cell=0x0070F7A0;
    inline constexpr std::uint32_t wxGameOverGameFlowStateInheritedSlot2C=0x005F15B0;
    inline constexpr std::uint32_t wxGameOverGameFlowStateSlot2CCell=0x0070F7A4;
    inline constexpr std::uint32_t wxGameOverGameFlowStateSlot38Cell=0x0070F7B0;
    inline constexpr std::uint32_t wxGameOverGameFlowStateSlot38Wrapper=0x005EFD40;
    inline constexpr std::uint32_t wxGameOverGameFlowStateSlot38CommonCallee=0x004F3DF0;
    inline constexpr std::uint32_t wxGameOverGameFlowStateSlot40Cell=0x0070F7B8;
    inline constexpr std::uint32_t wxGameOverGameFlowStateSharedSlot40=0x005D2BF0;
    inline constexpr std::uint32_t wxGameOverGameFlowStateEntry=0x005E04F0;
    inline constexpr std::uint32_t wxGameOverGameFlowStateEntryMenu44Cell=0x0070F7BC;
    inline constexpr std::uint32_t wxGameOverGameFlowStateEntryMenu44Target=0x005F1CD0;
    inline constexpr std::uint32_t wxGameOverGameFlowStateTick=0x005E0600;
}
