#pragma once
#include <cstdint>
namespace winx::evidence::ps2
{
    struct wxAnimationLoaderLayout final { std::uint8_t entity[0x130]; };
    static_assert(sizeof(wxAnimationLoaderLayout) == 0x130);
    inline constexpr std::uint32_t wxAnimationLoaderConstructor = 0x2A8870;
    inline constexpr std::uint32_t wxAnimationLoaderFactory = 0x3F73A0;
    inline constexpr std::uint32_t wxAnimationLoaderVtable = 0x49B410;
}
