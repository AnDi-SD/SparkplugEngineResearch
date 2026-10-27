#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace sparkplug::evidence::pc
{
    struct spPCProjectionFxLayout final
    {
        std::uint32_t primaryVtable;
        std::array<std::byte,0x14> secondaryObjectPrefix;
        std::uint32_t owner18;
        std::uint8_t initialized1C;
        std::array<std::byte,3> unassigned1D;
        std::uint32_t cache20;
        std::array<std::byte,0x84> unassigned24;
    };
    static_assert(sizeof(spPCProjectionFxLayout)==0xA8);
    static_assert(offsetof(spPCProjectionFxLayout,owner18)==0x18);
    static_assert(offsetof(spPCProjectionFxLayout,cache20)==0x20);
    inline constexpr std::uint32_t spPCProjectionFxFactory=0x4C51B0;
    inline constexpr std::uint32_t spPCProjectionFxPrimaryInit=0x4C4FF0;
    inline constexpr std::uint32_t spPCProjectionFxSecondaryClone=0x4C5220;
    inline constexpr std::uint32_t spPCProjectionFxPrimaryTable=0x6F2880;
    inline constexpr std::uint32_t spPCProjectionFxSecondaryTable=0x6F2864;
}
