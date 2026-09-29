#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxBacoProjectileManagerLayout final
    {
        std::uint8_t projectileManagerPrefix[0x1D0];
        std::uint32_t projectiles[2]; // 1D0, 1D4; owned
        std::uint32_t unknown1D8;
        std::uint32_t emitters[2]; // 1DC, 1E0; borrowed scene objects
    };
    static_assert(sizeof(wxBacoProjectileManagerLayout) == 0x1E4);
    static_assert(offsetof(wxBacoProjectileManagerLayout, projectiles) == 0x1D0);
    static_assert(offsetof(wxBacoProjectileManagerLayout, emitters) == 0x1DC);
    inline constexpr std::uint32_t wxBacoProjectileManagerClassID = 0x6F925DB7;
    inline constexpr std::uint32_t wxBacoProjectileManagerFactory = 0x004020C0;
    inline constexpr std::uint32_t wxBacoProjectileManagerVTable = 0x006F7D00;
    inline constexpr std::uint32_t wxBacoProjectileManagerNotify = 0x0050B170;
    inline constexpr std::uint32_t wxBacoProjectileManagerSetup = 0x0050FB80;
    inline constexpr std::uint32_t wxBacoProjectileManagerUpdate = 0x0050F680;
    inline constexpr std::uint32_t wxBacoProjectileManagerFire = 0x0050FA20;
    inline constexpr std::uint32_t wxBacoProjectileManagerFireAtPlayer = 0x0050FAB0;
}
