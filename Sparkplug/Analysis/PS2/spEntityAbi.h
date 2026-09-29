#pragma once

#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::ps2
{
    struct spEntityLayout final
    {
        std::uint8_t namedPrefix[0x14];
        std::uint32_t field14;
        std::uint32_t reference18;
        std::uint32_t field1C;
        std::uint32_t field20;
        std::uint32_t field24;
    };
    static_assert(sizeof(spEntityLayout) == 0x28);
    static_assert(offsetof(spEntityLayout, reference18) == 0x18);
    inline constexpr std::uint32_t spEntityFactory = 0x0014E560;
    inline constexpr std::uint32_t spEntityVTable = 0x0048DF60;
    inline constexpr std::uint32_t spEntitySetReference = 0x0014E150;
}
