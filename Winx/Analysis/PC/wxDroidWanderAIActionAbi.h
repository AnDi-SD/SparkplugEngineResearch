#pragma once
#include "wxAIActionAbi.h"

namespace winx::evidence::pc
{
    struct wxDroidWanderAIActionLayout final{wxAIActionLayout base;};
    static_assert(sizeof(wxDroidWanderAIActionLayout)==0x3A8);
    inline constexpr Address32 wxDroidWanderFactory=0x005BCF90;
    inline constexpr Address32 wxDroidWanderActualConstructionBase=0x0058FF00;
    inline constexpr Address32 wxDroidWanderVTable=0x00709EB8;
    inline constexpr Address32 wxDroidWanderGetter=0x005BCE20;
    inline constexpr Address32 wxDroidWanderRTTI=0x00767970;
    inline constexpr Address32 wxDroidWanderDeletingDestructor=0x005BD050;
    inline constexpr Address32 wxDroidWanderDestructor=0x005BCE30;
    inline constexpr Address32 wxDroidWanderCommandPublication=0x005BCE40;
    inline constexpr Address32 wxDroidWanderCommandClear=0x005BCE70;
    // Adjacent5BCE90 installs709E70, belongs to another class and supplies no
    // Droid constructor or field evidence. Actual Entry5BD070 is unqualified.
}
