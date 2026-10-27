#pragma once
#include "Analysis/PS2/wxAIActionAbi.h"
namespace winx::evidence::ps2
{
    struct wxFlyingWanderAIActionLayout final
    { wxAIActionLayout base;std::uint32_t words[12];std::uint8_t byte3DC,padding[3];std::uint32_t ownedNode; };
    static_assert(sizeof(wxFlyingWanderAIActionLayout)==0x3E4);
    static_assert(offsetof(wxFlyingWanderAIActionLayout,ownedNode)==0x3E0);
    inline constexpr std::uint32_t wxFlyingWanderFactory=0x24BE00;
    inline constexpr std::uint32_t wxFlyingWanderVtable=0x492590;
    inline constexpr std::uint32_t wxFlyingWanderEntry=0x24BAC0;
    inline constexpr std::uint32_t wxFlyingWanderQuery=0x24B930;
    inline constexpr std::uint32_t wxFlyingWanderCopy=0x24B9B0;
}
