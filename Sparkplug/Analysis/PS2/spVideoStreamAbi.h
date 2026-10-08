#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::ps2
{
    struct spVideoStreamObservedLayout final
    {
        std::uint32_t primaryVtable;
        std::array<std::uint8_t,0x14> crossPlatform;
        std::uint8_t byte18, byte19;
        std::array<std::uint8_t,2> unwritten1A;
        std::uint32_t ownedBuffer, word20;
    };
    static_assert(sizeof(spVideoStreamObservedLayout)==0x24);
    static_assert(offsetof(spVideoStreamObservedLayout,ownedBuffer)==0x1C);
    inline constexpr std::uint32_t VideoStreamConstructor=0x1C6150;
    inline constexpr std::uint32_t VideoStreamDestructor=0x1C60C0;
    inline constexpr std::uint32_t PS2VideoStreamFactory=0x20D8B0;
    inline constexpr std::uint32_t PS2VideoStreamClone=0x20D740;
    inline constexpr std::uint32_t PS2VideoStreamDestructor=0x20D6D0;
    inline constexpr std::uint32_t PS2VideoStreamPrimary=0x491E00;
    inline constexpr std::uint32_t PS2VideoStreamSecondary=0x491E30;
    inline constexpr std::array<std::uint32_t,10> PS2VideoStreamPrimaryLeaves{
        0x20D6C0,0x20D6B0,0x20D6A0,0x20D690,0x20D680,
        0x20D670,0x20D660,0x20D650,0x20D640,0x20D630};
    // Original+00 is a direct true leaf. There is no PC-style+04 dispatch
    // here, and the next+28 word belongs to secondary table metadata.
}
