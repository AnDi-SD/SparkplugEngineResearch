#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Native consumed fields only; this is not the portable C++ class layout.
    struct spCinematicEmptyLayout final
    {
        std::array<std::byte,0x10> basePrefix;
        std::uint32_t nameEntry10;
        std::uint32_t zero14;
        std::array<std::byte,4> unassigned18;
        std::uint32_t listSentinel1C;
        std::uint32_t listCount20;
    };
    static_assert(sizeof(spCinematicEmptyLayout)==0x24);
    static_assert(offsetof(spCinematicEmptyLayout,nameEntry10)==0x10);
    static_assert(offsetof(spCinematicEmptyLayout,listSentinel1C)==0x1C);
    inline constexpr std::uint32_t spCinematicFactory=0x4532F0;
    inline constexpr std::uint32_t spCinematicTable=0x6E6DD0;
    inline constexpr std::uint32_t spCinematicGetter=0x452F60;
    inline constexpr std::uint32_t spCinematicRecord=0x75F7C0;
    inline constexpr std::uint32_t spCinematicDelete=0x4530D0;
    inline constexpr std::uint32_t spCinematicDestructor=0x452E80;
}
