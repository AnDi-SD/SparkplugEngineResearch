#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxProjectileLayout final
    {
        std::uint8_t bytes[0xEC];
    };
    static_assert(sizeof(wxProjectileLayout) == 0xEC);
    inline constexpr std::uint32_t wxProjectileFactory = 0x003F6FA0;
    inline constexpr std::uint32_t wxProjectileConstructor = 0x002C5030;
    inline constexpr std::uint32_t wxProjectileVTable = 0x0049B2B0;
    inline constexpr std::uint32_t wxProjectileCopy = 0x002C3450;
    inline constexpr std::size_t wxProjectileActorOffset = 0x20;
}
