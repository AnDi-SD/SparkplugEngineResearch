#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxWebSpitProjectileLayout final
    {
        std::uint8_t projectilePrefix[0xEC];
        std::uint8_t enabledEC;
        std::uint8_t gapED[3];
        std::uint32_t fieldF0;
        std::uint32_t references[2];
        std::uint32_t fieldFC;
    };
    static_assert(sizeof(wxWebSpitProjectileLayout) == 0x100);
    static_assert(offsetof(wxWebSpitProjectileLayout, references) == 0xF4);
    inline constexpr std::uint32_t wxWebSpitProjectileFactory = 0x003F6CA0;
    inline constexpr std::uint32_t wxWebSpitProjectileConstructor = 0x002F3B70;
    inline constexpr std::uint32_t wxWebSpitProjectileVTable = 0x0049B1F0;
    inline constexpr std::uint32_t wxWebSpitProjectileCopy = 0x002F2F50;
}
