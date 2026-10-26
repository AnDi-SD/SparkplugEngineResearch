#pragma once
#include "wxGameFlowStateAbi.h"
namespace winx::evidence::pc
{
    struct wxMenuGameFlowStateLayout final
    {
        wxGameFlowStateLayout base;
        std::uint32_t allocation3C,view40,mode44,word48,object4C,timer50,borrowedInput54;
        std::uint8_t field58,unwritten59[3];
    };
    static_assert(sizeof(wxMenuGameFlowStateLayout)==0x5C);
    static_assert(offsetof(wxMenuGameFlowStateLayout,timer50)==0x50);
    inline constexpr std::uint32_t wxMenuGameFlowStateFactory=0x005F1AE0;
    inline constexpr std::uint32_t wxMenuGameFlowStateConstructor=0x005F1260;
    inline constexpr std::uint32_t wxMenuGameFlowStateVTable=0x007104A8;
    inline constexpr std::uint32_t wxMenuGameFlowStateRTTI=0x00768B10;
    inline constexpr std::uint32_t wxMenuGameFlowStateExit=0x005F14B0;
    inline constexpr std::uint32_t wxMenuGameFlowStateSlot28=0x005F16D0;
    inline constexpr std::uint32_t wxMenuGameFlowStateSlot2C=0x005F15B0;
}
