#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxVectorWrapperLayout final
    {
        std::uint8_t base[0x10];
        std::uint32_t components[3];
    };
    static_assert(sizeof(wxVectorWrapperLayout) == 0x1C);
    static_assert(offsetof(wxVectorWrapperLayout, components) == 0x10);

    inline constexpr std::uint32_t wxVectorWrapperClassID = 0x1F793E0D;
    inline constexpr std::uint32_t wxVectorWrapperFactory = 0x00596DF0;
    inline constexpr std::uint32_t wxVectorWrapperVTable = 0x00702FFC;
    inline constexpr std::uint32_t wxVectorWrapperCopy = 0x00596DA0;
    inline constexpr std::uint32_t wxVectorWrapperClone = 0x00596E60;
    inline constexpr std::uint32_t wxVectorWrapperRTTI = 0x007661A8;
}
