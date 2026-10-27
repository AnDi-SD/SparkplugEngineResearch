#pragma once
#include "Analysis/PC/wxAIActionAbi.h"
namespace winx::evidence::pc
{
    struct wxFlyingWanderAIActionLayout final
    { wxAIActionLayout base;std::uint32_t words[12];std::uint8_t byte3D8,padding[3];std::uint32_t ownedNode; };
    static_assert(sizeof(wxFlyingWanderAIActionLayout)==0x3E0);
    static_assert(offsetof(wxFlyingWanderAIActionLayout,ownedNode)==0x3DC);
    inline constexpr std::uint32_t wxFlyingWanderFactory=0x5B5410;
    inline constexpr std::uint32_t wxFlyingWanderConstructor=0x5B5220;
    inline constexpr std::uint32_t wxFlyingWanderDestructor=0x5B5350;
    inline constexpr std::uint32_t wxFlyingWanderVtable=0x709748;
    inline constexpr std::uint32_t wxFlyingWanderCopy=0x5B5140;
    inline constexpr std::uint32_t wxFlyingWanderEntry=0x5B50E0;
    inline constexpr std::uint32_t wxFlyingWanderQuery=0x5B5100;
    inline constexpr std::uint32_t wxFlyingWanderBorrowedDefaultVector=0x7600E0;
}
