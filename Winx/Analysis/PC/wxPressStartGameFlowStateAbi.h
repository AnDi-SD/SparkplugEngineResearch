#pragma once
#include "wxMenuGameFlowStateAbi.h"
#include "Analysis/PC/spTimerAbi.h"

namespace winx::evidence::pc
{
    struct wxPressStartGameFlowStateLayout final
    {
        wxMenuGameFlowStateLayout base;
        std::uint32_t borrowedInput5C;
        std::uint8_t field60,unwritten61[3];
        std::uint32_t unwritten64;
        sparkplug::analysis::pc::spTimerState32 timer68;
    };
    static_assert(sizeof(wxPressStartGameFlowStateLayout)==0x8C);
    static_assert(offsetof(wxPressStartGameFlowStateLayout,borrowedInput5C)==0x5C);
    static_assert(offsetof(wxPressStartGameFlowStateLayout,field60)==0x60);
    static_assert(offsetof(wxPressStartGameFlowStateLayout,unwritten64)==0x64);
    static_assert(offsetof(wxPressStartGameFlowStateLayout,timer68)==0x68);
    inline constexpr std::uint32_t wxPressStartGameFlowStateFactory=0x005EFFD0;
    inline constexpr std::uint32_t wxPressStartGameFlowStateConstructor=0x005EFD60;
    inline constexpr std::uint32_t wxPressStartGameFlowStateDestructor=0x005EFCD0;
    inline constexpr std::uint32_t wxPressStartGameFlowStateDeletingDestructor=0x005EFDE0;
    inline constexpr std::uint32_t wxPressStartGameFlowStateClone=0x005F0030;
    inline constexpr std::uint32_t wxPressStartGameFlowStateVTable=0x00710328;
    inline constexpr std::uint32_t wxPressStartGameFlowStateRTTI=0x007689F0;
    inline constexpr std::uint32_t wxPressStartGameFlowStateGetter=0x005EFD30;
    inline constexpr std::uint32_t wxPressStartGameFlowStateEntry=0x005EFE00;
    inline constexpr std::uint32_t wxPressStartGameFlowStateExit=0x005EFEF0;
    // Exit's5EFF1C full DWORD scratch store makes tail0;5EFF2D writes payload9C.
    // 40EC00 receives code27D1,key11,payload9C,tail0; key occupies packet0C.
    // Parent5F14B0 builds directed2764 frame inline, without435D30.
    inline constexpr std::uint32_t wxPressStartGameFlowStateTick=0x005EFF40;
}
