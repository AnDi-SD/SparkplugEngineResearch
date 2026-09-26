#pragma once
#include <cstdint>
#include <cstddef>
namespace winx::evidence::pc
{
    struct wxAnimationControllerLayout final
    {
        std::uint8_t entity[0x124];
        std::uint32_t character, actor;
        std::uint8_t request[0x44];
        std::uint32_t mark9, mark3, old, recent, count;
        std::uint8_t forceEnable, padding[3];
    };
    static_assert(sizeof(wxAnimationControllerLayout)==0x188);
    static_assert(offsetof(wxAnimationControllerLayout,request)==0x12C);
    static_assert(offsetof(wxAnimationControllerLayout,forceEnable)==0x184);
    inline constexpr std::uint32_t wxAnimationControllerConstructor=0x4FB210;
    inline constexpr std::uint32_t wxAnimationControllerFactory=0x401700;
    inline constexpr std::uint32_t wxAnimationControllerVtable=0x6F670C;
}
