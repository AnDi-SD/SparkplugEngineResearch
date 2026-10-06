#pragma once
#include <cstddef>
#include <cstdint>

namespace sparkplug::analysis::pc
{
    struct spQuad3DState32 final
    {
        std::uint32_t vtable, base04;
        std::uint16_t base08;
        std::uint8_t padding0A[2];
        std::uint32_t base0C, vertices10, material14;
        float position18[3], width24, height28;
        std::uint32_t color2C;
    };
    static_assert(sizeof(spQuad3DState32) == 0x30);
    static_assert(offsetof(spQuad3DState32, position18) == 0x18);
    static_assert(offsetof(spQuad3DState32, width24) == 0x24);
    static_assert(offsetof(spQuad3DState32, color2C) == 0x2C);
    inline constexpr std::uint32_t Quad3DFactory = 0x004CEF60;
    inline constexpr std::uint32_t Quad3DDraw = 0x004CEB00;
    inline constexpr std::uint32_t Quad3DDestructor = 0x004CEA80;
    inline constexpr std::uint32_t Quad3DClone = 0x004CEFC0;
    inline constexpr std::uint32_t Quad3DGetter = 0x004CEA70;
    inline constexpr std::uint32_t Quad3DVtable = 0x006F3558;
}
