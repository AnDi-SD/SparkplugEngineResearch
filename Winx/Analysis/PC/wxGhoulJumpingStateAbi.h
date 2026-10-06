#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::pc
{
    using wxGhoulJumpingStateLayout=wxCharacterStateLayout;
    static_assert(sizeof(wxGhoulJumpingStateLayout)==0x3C);
    inline constexpr Address32 GhoulJumpingTable=0x006F92F0;
    inline constexpr Address32 GhoulJumpingEntry=0x0051ADC0;
    inline constexpr Address32 GhoulJumpingHook2C=0x0051EC80;
    inline constexpr Address32 GhoulJumpingPermission=0x0051AD00;
    inline constexpr Address32 GhoulJumpingHook38=0x0051AD20;
    inline constexpr Address32 GhoulJumpingEvent=0x0051AD60;
}
