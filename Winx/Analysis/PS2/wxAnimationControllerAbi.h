#pragma once
#include <cstdint>
#include <cstddef>
namespace winx::evidence::ps2
{
    struct wxAnimationControllerLayout final
    {
        std::uint8_t entity[0x130];
        std::uint32_t character, actor;
        std::uint8_t request[0x44];
        std::uint32_t mark9, mark3, old, recent, count;
        std::uint8_t forceEnable, padding[15];
    };
    static_assert(sizeof(wxAnimationControllerLayout)==0x1A0);
    static_assert(offsetof(wxAnimationControllerLayout,request)==0x138);
    static_assert(offsetof(wxAnimationControllerLayout,forceEnable)==0x190);
    inline constexpr std::uint32_t wxAnimationControllerConstructor=0x2A7290;
    inline constexpr std::uint32_t wxAnimationControllerFactory=0x3F74A0;
    inline constexpr std::uint32_t wxAnimationControllerVtable=0x49B450;
}
