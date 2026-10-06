#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::pc
{
    using wxPullLeverStateLayout=wxCharacterStateLayout;
    static_assert(sizeof(wxPullLeverStateLayout)==0x3C);
    inline constexpr Address32 PullLeverTable=0x006F94E0;
    inline constexpr Address32 PullLeverEntry=0x0051ECD0;
    inline constexpr Address32 PullLeverExit=0x0051EDF0;
    inline constexpr Address32 PullLeverHook2C=0x0051EC80;
    inline constexpr Address32 PullLeverPermission=0x00523770;
}
