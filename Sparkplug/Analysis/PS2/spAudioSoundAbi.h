#pragma once
#include <cstddef>
#include <cstdint>

namespace sparkplug::analysis::ps2
{
    struct spAudioSoundState32 final
    {
        std::uint8_t nodePrefix[0xC0];
        std::uint32_t tailC0[17];
        std::uint8_t untouched104[12];
    };
    static_assert(sizeof(spAudioSoundState32) == 0x110);
    static_assert(offsetof(spAudioSoundState32, tailC0) == 0xC0);
    static_assert(offsetof(spAudioSoundState32, tailC0) + 5 * sizeof(std::uint32_t) == 0xD4);
    static_assert(offsetof(spAudioSoundState32, tailC0) + 6 * sizeof(std::uint32_t) == 0xD8);
    static_assert(offsetof(spAudioSoundState32, untouched104) == 0x104);
    inline constexpr std::uint32_t AudioSoundFactory = 0x001240D0;
    inline constexpr std::uint32_t AudioSoundConstructor = 0x00123F80;
    inline constexpr std::uint32_t AudioSoundScalar = 0x00123E70;
    inline constexpr std::uint32_t AudioSoundCombinedScalar = 0x00123E80;
    inline constexpr std::uint32_t AudioSoundConfigure = 0x00123EC0;
    inline constexpr std::uint32_t AudioSoundClone = 0x00124000;
    inline constexpr std::uint32_t AudioSoundDelete = 0x00123F20;
    inline constexpr std::uint32_t AudioSoundVtable = 0x0048D0D0;
}
