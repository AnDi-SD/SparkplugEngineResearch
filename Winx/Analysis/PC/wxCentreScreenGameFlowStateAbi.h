#pragma once
#include "wxGameFlowStateAbi.h"
namespace winx::evidence::pc
{
    struct wxCentreScreenGameFlowStateLayout final
    {
        wxGameFlowStateLayout base;
        std::uint32_t material3C,word40,word44; // all unwritten by constructor
    };
    static_assert(sizeof(wxCentreScreenGameFlowStateLayout)==0x48);
    static_assert(offsetof(wxCentreScreenGameFlowStateLayout,material3C)==0x3C);
    inline constexpr std::uint32_t wxCentreScreenGameFlowStateFactory=0x005CE840;
    inline constexpr std::uint32_t wxCentreScreenGameFlowStateVTable=0x0070AC2C;
    inline constexpr std::uint32_t wxCentreScreenGameFlowStateRTTI=0x00767C78;
    inline constexpr std::uint32_t wxCentreScreenGameFlowStateEntry=0x005CE920;
    inline constexpr std::uint32_t wxCentreScreenGameFlowStateExit=0x005CE9D0;
}
