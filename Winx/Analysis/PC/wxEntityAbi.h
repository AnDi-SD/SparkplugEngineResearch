#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxEntityLayout final
    {
        std::uint8_t spEntityPrefix[0x28];
        std::uint8_t ownBytes[0xFC];
    };
    static_assert(sizeof(wxEntityLayout) == 0x124);
    static_assert(offsetof(wxEntityLayout, ownBytes) == 0x28);
    inline constexpr std::uint32_t wxEntityFactory = 0x00401110;
    inline constexpr std::uint32_t wxEntityConstructor = 0x004DA7C0;
    inline constexpr std::uint32_t wxEntityVTable = 0x006F4EB8;
    inline constexpr std::uint32_t wxEntityCopy = 0x004D9700;
    inline constexpr std::uint32_t wxEntityFlags = 0x004DA930;
}
