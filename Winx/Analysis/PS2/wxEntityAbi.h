#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxEntityLayout final
    {
        std::uint8_t spEntityPrefix[0x28];
        std::uint8_t ownBytes[0x108];
    };
    static_assert(sizeof(wxEntityLayout) == 0x130);
    static_assert(offsetof(wxEntityLayout, ownBytes) == 0x28);
    inline constexpr std::uint32_t wxEntityFactory = 0x003F8570;
    inline constexpr std::uint32_t wxEntityConstructor = 0x00286CA0;
    inline constexpr std::uint32_t wxEntityVTable = 0x0049B8D0;
    inline constexpr std::uint32_t wxEntityCopy = 0x00286AB0;
    inline constexpr std::uint32_t wxEntityFlags = 0x00286460;
}
