#pragma once
#include "wxGameFlowStateAbi.h"
namespace winx::evidence::pc
{
    struct wxCreditsMenuGameFlowStateLayout final
    {
        wxGameFlowStateLayout base;
        std::uint32_t borrowedInput3C;
        std::uint8_t field40,field41,unwritten42[2];
    };
    static_assert(sizeof(wxCreditsMenuGameFlowStateLayout)==0x44);
    static_assert(offsetof(wxCreditsMenuGameFlowStateLayout,borrowedInput3C)==0x3C);
    static_assert(offsetof(wxCreditsMenuGameFlowStateLayout,field40)==0x40);
    inline constexpr std::uint32_t wxCreditsMenuGameFlowStateFactory=0x005EC620;
    inline constexpr std::uint32_t wxCreditsMenuGameFlowStateConstructor=0x005EC3E0;
    inline constexpr std::uint32_t wxCreditsMenuGameFlowStateVTable=0x00710078;
    inline constexpr std::uint32_t wxCreditsMenuGameFlowStateRTTI=0x00768870;
    inline constexpr std::uint32_t wxCreditsMenuGameFlowStateComplete=0x005EC5A0;
    inline constexpr std::uint32_t wxCreditsMenuGameFlowStateEntry=0x005EC4F0;
    inline constexpr std::uint32_t wxCreditsMenuGameFlowStateExit=0x005EC570;
    inline constexpr std::uint32_t wxCreditsMenuGameFlowStateTick=0x005EC720;
}
