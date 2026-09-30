#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxStingerProjectileLayout final
    {
        std::uint8_t projectilePrefix[0xEC];
        std::uint32_t references[2];
        std::uint32_t fieldF4;
    };
    static_assert(sizeof(wxStingerProjectileLayout) == 0xF8);
    static_assert(offsetof(wxStingerProjectileLayout, references) == 0xEC);
    inline constexpr std::uint32_t wxStingerProjectileFactory = 0x003F6BA0;
    inline constexpr std::uint32_t wxStingerProjectileConstructor = 0x002F6D10;
    inline constexpr std::uint32_t wxStingerProjectileVTable = 0x0049B1B0;
    inline constexpr std::uint32_t wxStingerProjectileCopy = 0x002F64B0;
}
