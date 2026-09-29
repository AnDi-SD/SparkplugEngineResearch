#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxBacoProjectileManagerLayout final
    {
        std::uint8_t projectileManagerPrefix[0x1E0];
        std::uint32_t projectiles[2]; // 1E0, 1E4
        std::uint32_t unknown1E8;
        std::uint32_t emitters[2]; // 1EC, 1F0
        std::uint8_t unknownTail[0x0C];
    };
    static_assert(sizeof(wxBacoProjectileManagerLayout) == 0x200);
    static_assert(offsetof(wxBacoProjectileManagerLayout, projectiles) == 0x1E0);
    static_assert(offsetof(wxBacoProjectileManagerLayout, emitters) == 0x1EC);
    inline constexpr std::uint32_t wxBacoProjectileManagerClassID = 0x6F925DB7;
    inline constexpr std::uint32_t wxBacoProjectileManagerConstructor = 0x002FEB50;
    inline constexpr std::uint32_t wxBacoProjectileManagerVTable = 0x0049ACA0;
    inline constexpr std::uint32_t wxBacoProjectileManagerNotify = 0x002FE9C0;
    inline constexpr std::uint32_t wxBacoProjectileManagerSetup = 0x002FE8F0;
    inline constexpr std::uint32_t wxBacoProjectileManagerUpdate = 0x002FE890;
    inline constexpr std::uint32_t wxBacoProjectileManagerFire = 0x002FE3E0;
    inline constexpr std::uint32_t wxBacoProjectileManagerFireAtPlayer = 0x002FE520;
}
