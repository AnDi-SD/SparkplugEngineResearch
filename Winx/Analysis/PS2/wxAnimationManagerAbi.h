#pragma once
#include <cstdint>
#include <cstddef>
namespace winx::evidence::ps2
{
    struct wxAnimationManagerLayout final
    {
        std::uint8_t base[0x10];
        std::uint32_t singletonVtable;
        std::uint8_t cache[0x10];
        std::uint8_t tables[68][0x10];
    };
    static_assert(sizeof(wxAnimationManagerLayout) == 0x464);
    static_assert(offsetof(wxAnimationManagerLayout, tables) == 0x24);
    inline constexpr std::uint32_t wxAnimationManagerFactory = 0x274650; // inlined constructor
    inline constexpr std::uint32_t wxAnimationManagerVtable = 0x492FF0;
}
