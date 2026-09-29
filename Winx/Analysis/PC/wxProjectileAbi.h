#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxProjectileLayout final
    {
        std::uint8_t bytes[0xEC];
    };
    static_assert(sizeof(wxProjectileLayout) == 0xEC);
    inline constexpr std::uint32_t wxProjectileFactory = 0x004018E0;
    inline constexpr std::uint32_t wxProjectileVTable = 0x006F6B04;
    inline constexpr std::uint32_t wxProjectileCopy = 0x004FFC10;
    inline constexpr std::size_t wxProjectileActorOffset = 0x20;
}
