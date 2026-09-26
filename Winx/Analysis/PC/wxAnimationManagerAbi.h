#pragma once
#include <cstdint>
#include <cstddef>
namespace winx::evidence::pc
{
    struct wxAnimationManagerLayout final
    {
        std::uint8_t base[0x10];
        std::uint32_t singletonVtable;
        std::uint8_t cache[0x0C];
        std::uint8_t tables[68][0x0C];
    };
    static_assert(sizeof(wxAnimationManagerLayout) == 0x350);
    static_assert(offsetof(wxAnimationManagerLayout, tables) == 0x20);
    inline constexpr std::uint32_t wxAnimationManagerConstructor = 0x59C970;
    inline constexpr std::uint32_t wxAnimationManagerFactory = 0x59CA40;
    inline constexpr std::uint32_t wxAnimationManagerVtable = 0x703470;
}
