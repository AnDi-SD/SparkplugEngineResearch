#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxProjectileManagerLayout final
    {
        std::uint8_t wxEntityPrefix[0x124];
        std::uint8_t ownWordsAndGaps[0x4C];
        std::uint32_t pool[4][5];
        std::uint8_t tail[0x10];
    };
    static_assert(sizeof(wxProjectileManagerLayout) == 0x1D0);
    static_assert(offsetof(wxProjectileManagerLayout, pool) == 0x170);
    static_assert(offsetof(wxProjectileManagerLayout, tail) == 0x1C0);
    inline constexpr std::uint32_t wxProjectileManagerFactory = 0x00401D00;
    inline constexpr std::uint32_t wxProjectileManagerVTable = 0x006F7008;
    inline constexpr std::uint32_t wxProjectileManagerNotify = 0x005061E0;
    inline constexpr std::uint32_t wxProjectileManagerCopy = 0x00505F10;
    inline constexpr std::uint32_t wxProjectileManagerRegister = 0x00506100;
    inline constexpr std::uint32_t wxProjectileManagerTick = 0x005068A0;
}
