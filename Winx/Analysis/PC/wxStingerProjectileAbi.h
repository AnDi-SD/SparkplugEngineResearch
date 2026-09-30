#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxStingerProjectileLayout final
    {
        std::uint8_t projectilePrefix[0xEC];
        std::uint32_t references[2];
        std::uint32_t fieldF4;
    };
    static_assert(sizeof(wxStingerProjectileLayout) == 0xF8);
    static_assert(offsetof(wxStingerProjectileLayout, references) == 0xEC);
    inline constexpr std::uint32_t wxStingerProjectileFactory = 0x00401A60;
    inline constexpr std::uint32_t wxStingerProjectileVTable = 0x006F6C7C;
    inline constexpr std::uint32_t wxStingerProjectileCopy = 0x00501F30;
}
