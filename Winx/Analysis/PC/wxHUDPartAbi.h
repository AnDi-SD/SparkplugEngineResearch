#pragma once
#include "wxEntityAbi.h"

namespace winx::evidence::pc
{
    struct wxHUDPartLayout final
    {
        wxEntityLayout entity;
        std::uint32_t allocator124;
        std::uint32_t begin,end,limit;
        std::uint32_t initial,selected,key,layer;
        std::uint8_t flag144,flag145,initialized,padding147;
    };
    static_assert(sizeof(wxHUDPartLayout)==0x148);
    static_assert(offsetof(wxHUDPartLayout,begin)==0x128);
    static_assert(offsetof(wxHUDPartLayout,initial)==0x134);
    static_assert(offsetof(wxHUDPartLayout,initialized)==0x146);
    inline constexpr std::uint32_t wxHUDPartFactory=0x00406800;
    inline constexpr std::uint32_t wxHUDPartConstructor=0x0055d8e0;
    inline constexpr std::uint32_t wxHUDPartTable=0x006fefac;
    inline constexpr std::uint32_t wxHUDPartRecord=0x00753948;
    inline constexpr std::uint32_t wxHUDPartNotify=0x0055db50;
    inline constexpr std::uint32_t wxHUDPartFind=0x0055d3f0;
    inline constexpr std::uint32_t wxHUDPartSelect=0x0055d370;
    inline constexpr std::uint32_t wxHUDPartCopy=0x0055d9f0;
    inline constexpr std::uint32_t wxHUDPartDestroyBody=0x0055d5a0;
}
