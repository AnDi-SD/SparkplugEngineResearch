#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxProjectileManagerLayout final
    {
        std::uint8_t wxEntityPrefix[0x130];
        std::uint8_t ownWordsAndGaps[0x4C];
        std::uint32_t pool[4][5];
        std::uint8_t tail[0x14];
    };
    static_assert(sizeof(wxProjectileManagerLayout) == 0x1E0);
    static_assert(offsetof(wxProjectileManagerLayout, pool) == 0x17C);
    static_assert(offsetof(wxProjectileManagerLayout, tail) == 0x1CC);
    inline constexpr std::uint32_t wxProjectileManagerFactory = 0x003F64A0;
    inline constexpr std::uint32_t wxProjectileManagerConstructor = 0x002C6720;
    inline constexpr std::uint32_t wxProjectileManagerVTable = 0x0049AFD0;
    inline constexpr std::uint32_t wxProjectileManagerNotify = 0x002C6590;
    inline constexpr std::uint32_t wxProjectileManagerCopy = 0x002C6290;
    inline constexpr std::uint32_t wxProjectileManagerRegister = 0x002C5120;
    inline constexpr std::uint32_t wxProjectileManagerTick = 0x002C6370;
}
