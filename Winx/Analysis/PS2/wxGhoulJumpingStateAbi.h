#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::ps2
{
    using wxGhoulJumpingStateLayout=wxCharacterStateLayout;
    static_assert(sizeof(wxGhoulJumpingStateLayout)==0x3C);
    inline constexpr Address32 GhoulJumpingTable=0x00499B80;
    inline constexpr Address32 GhoulJumpingConstructor=0x002E50E0;
    inline constexpr Address32 GhoulJumpingEntry=0x002E4E00;
    inline constexpr Address32 GhoulJumpingHook2C=0x002E4F50;
    inline constexpr Address32 GhoulJumpingPermission=0x002E4FD0;
    inline constexpr Address32 GhoulJumpingHook38=0x002E4FA0;
    inline constexpr Address32 GhoulJumpingEvent=0x002E5010;
}
