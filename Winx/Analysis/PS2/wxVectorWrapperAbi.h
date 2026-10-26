#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxVectorWrapperLayout final
    {
        std::uint8_t base[0x10];
        std::uint32_t components[3];
    };
    static_assert(sizeof(wxVectorWrapperLayout) == 0x1C);
    static_assert(offsetof(wxVectorWrapperLayout, components) == 0x10);

    inline constexpr std::uint32_t wxVectorWrapperClassID = 0x1F793E0D;
    inline constexpr std::uint32_t wxVectorWrapperFactory = 0x002A5130;
    inline constexpr std::uint32_t wxVectorWrapperConstructor = 0x002A5010;
    inline constexpr std::uint32_t wxVectorWrapperVTable = 0x004933A0;
    inline constexpr std::uint32_t wxVectorWrapperCopy = 0x002A4F20;
    inline constexpr std::uint32_t wxVectorWrapperClone = 0x002A5050;
    inline constexpr std::uint32_t wxVectorWrapperRTTI = 0x004C3510;
}
