#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::ps2
{
    using wxPullLeverStateLayout=wxCharacterStateLayout;
    static_assert(sizeof(wxPullLeverStateLayout)==0x3C);
    inline constexpr Address32 PullLeverTable=0x00499A90;
    inline constexpr Address32 PullLeverConstructor=0x002D2620;
    inline constexpr Address32 PullLeverEntry=0x002D22E0;
    inline constexpr Address32 PullLeverExit=0x002D2220;
    inline constexpr Address32 PullLeverHook2C=0x002D2550;
    inline constexpr Address32 PullLeverPermission=0x002D25B0;
}
