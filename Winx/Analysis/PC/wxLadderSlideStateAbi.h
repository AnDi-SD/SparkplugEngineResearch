#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::pc
{
    using wxLadderSlideStateLayout = wxCharacterStateLayout;
    static_assert(sizeof(wxLadderSlideStateLayout) == 0x3c);
    inline constexpr Address32 LadderSlideTable = 0x006f8658;
    inline constexpr Address32 LadderSlideFactory = 0x00402960;
    inline constexpr Address32 LadderSlideClone = 0x005164e0;
    inline constexpr Address32 LadderSlideEntry = 0x005165a0;
    inline constexpr Address32 LadderSlideExit = 0x00516640;
    inline constexpr Address32 LadderSlideUpdate = 0x00516540;
    inline constexpr Address32 LadderSlidePermission = 0x00516500;
    inline constexpr Address32 LadderSlideTransitionPermission = 0x00513480;
    inline constexpr std::uint32_t LadderSlideOwnerFlagsOffset = 0x144;
}
