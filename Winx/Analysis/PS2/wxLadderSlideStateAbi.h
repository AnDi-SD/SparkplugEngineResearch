#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::ps2
{
    using wxLadderSlideStateLayout = wxCharacterStateLayout;
    static_assert(sizeof(wxLadderSlideStateLayout) == 0x3c);
    inline constexpr Address32 LadderSlideTable = 0x0049a580;
    inline constexpr Address32 LadderSlideFactory = 0x003f43a0;
    inline constexpr Address32 LadderSlideClone = 0x003f42e0;
    inline constexpr Address32 LadderSlideConstructor = 0x002cf0a0;
    inline constexpr Address32 LadderSlideEntry = 0x002ced60;
    inline constexpr Address32 LadderSlideExit = 0x002cebb0;
    inline constexpr Address32 LadderSlideUpdate = 0x002ceec0;
    inline constexpr Address32 LadderSlidePermission = 0x002cf010;
    inline constexpr Address32 LadderSlideTransitionPermission = 0x002cf000;
    inline constexpr std::uint32_t LadderSlideOwnerFlagsOffset = 0x150;
}
