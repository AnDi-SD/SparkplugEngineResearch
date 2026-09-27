#pragma once
#include <cstddef>
#include <cstdint>
namespace winx::evidence::ps2
{
    struct wxArrowTrapLayout final
    {
        std::uint8_t wxEntity[0x130];
        std::uint8_t active, pad131[3];
        float speed;
        std::uint32_t pauseMilliseconds;
        float intervalSeconds;
        std::uint32_t unknown140;
        std::uint32_t arrows[3], emitters[3];
        float arrowPositions[3][3], phase[3], timer[3];
        std::uint32_t components[3];
        std::uint8_t inRange, tail[11];
    };
    static_assert(offsetof(wxArrowTrapLayout, speed) == 0x134);
    static_assert(offsetof(wxArrowTrapLayout, arrows) == 0x144);
    static_assert(offsetof(wxArrowTrapLayout, phase) == 0x180);
    static_assert(offsetof(wxArrowTrapLayout, inRange) == 0x1A4);
    static_assert(sizeof(wxArrowTrapLayout) == 0x1B0);
    inline constexpr std::uint32_t wxArrowTrapFactory = 0x3E6A90;
    inline constexpr std::uint32_t wxArrowTrapConstructor = 0x3835D0;
    inline constexpr std::uint32_t wxArrowTrapVtable = 0x4955D0;
}
