#pragma once
#include "wxGameFlowStateAbi.h"
#include "Analysis/PC/spTimerAbi.h"
namespace winx::evidence::pc
{
    struct wxLogoAnimGameFlowStateLayout final
    {
        wxGameFlowStateLayout base;
        std::uint32_t borrowedInput3C;
        std::uint8_t field40,field41,unwritten42[2];
        std::uint32_t word44,word48,word4C,referenced50,word54;
        sparkplug::analysis::pc::spTimerState32 timer58;
        std::uint32_t word7C,word80,unwritten84;
    };
    static_assert(sizeof(wxLogoAnimGameFlowStateLayout)==0x88);
    static_assert(offsetof(wxLogoAnimGameFlowStateLayout,timer58)==0x58);
    inline constexpr std::uint32_t wxLogoAnimGameFlowStateFactory=0x005CE770;
    inline constexpr std::uint32_t wxLogoAnimGameFlowStateConstructor=0x005CE3E0;
    inline constexpr std::uint32_t wxLogoAnimGameFlowStateVTable=0x0070ABB4;
    inline constexpr std::uint32_t wxLogoAnimGameFlowStateRTTI=0x00767C18;
    inline constexpr std::uint32_t wxLogoAnimGameFlowStateEntry=0x005CDD80;
    inline constexpr std::uint32_t wxLogoAnimGameFlowStateExit=0x005CDE60;
    inline constexpr std::uint32_t wxLogoSharedResourceLoading=0x00597480;
}
