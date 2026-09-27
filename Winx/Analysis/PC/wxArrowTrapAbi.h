#pragma once
#include <cstddef>
#include <cstdint>
namespace winx::evidence::pc
{
    struct wxArrowTrapLayout final
    {
        std::uint8_t wxEntity[0x124];
        std::uint8_t active, pad125[3];
        float speed;
        std::uint32_t pauseMilliseconds;
        float intervalSeconds;
        std::uint32_t unknown134;
        std::uint32_t arrows[3], emitters[3];
        float arrowPositions[3][3], phase[3], timer[3];
        std::uint32_t components[3];
        std::uint8_t inRange, tail[3];
    };
    static_assert(offsetof(wxArrowTrapLayout, speed) == 0x128);
    static_assert(offsetof(wxArrowTrapLayout, arrows) == 0x138);
    static_assert(offsetof(wxArrowTrapLayout, phase) == 0x174);
    static_assert(offsetof(wxArrowTrapLayout, inRange) == 0x198);
    static_assert(sizeof(wxArrowTrapLayout) == 0x19C);
    inline constexpr std::uint32_t wxArrowTrapFactory = 0x407A60;
    inline constexpr std::uint32_t wxArrowTrapConstructor = 0x580BF0;
    inline constexpr std::uint32_t wxArrowTrapVtable = 0x701638;
}
