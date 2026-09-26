#pragma once
#include <cstdint>
namespace winx::evidence::pc
{
    // No fields beyond the physical wxEntity parent.
    struct wxAnimationLoaderLayout final { std::uint8_t entity[0x124]; };
    static_assert(sizeof(wxAnimationLoaderLayout) == 0x124);
    inline constexpr std::uint32_t wxAnimationLoaderConstructor = 0x4FB860;
    inline constexpr std::uint32_t wxAnimationLoaderFactory = 0x401760;
    inline constexpr std::uint32_t wxAnimationLoaderVtable = 0x6F6744;
}
