#pragma once
#include <cstddef>
#include <cstdint>

namespace sparkplug::analysis::pc
{
    struct spAudioSoundState32 final
    {
        std::uint8_t nodePrefix[0xB4];
        std::uint32_t fieldB4, fieldB8, fieldBC;
        float fieldC0, fieldC4, scalarC8;
        std::uint32_t groupCC, fieldD0;
        float fieldD4;
        std::uint32_t fieldD8, fieldDC, fieldE0, fieldE4, fieldE8, fieldEC, fieldF0, fieldF4;
    };
    static_assert(sizeof(spAudioSoundState32) == 0xF8);
    static_assert(offsetof(spAudioSoundState32, scalarC8) == 0xC8);
    static_assert(offsetof(spAudioSoundState32, groupCC) == 0xCC);
    inline constexpr std::uint32_t AudioSoundFactory = 0x004A29A0;
    inline constexpr std::uint32_t AudioSoundConstructor = 0x0051B5F0;
    inline constexpr std::uint32_t AudioSoundScalar = 0x004A2990;
    inline constexpr std::uint32_t AudioSoundCombinedScalar = 0x004A2AD0;
    inline constexpr std::uint32_t AudioSoundClone = 0x004A2A00;
    inline constexpr std::uint32_t AudioSoundDelete = 0x004A2A50;
    inline constexpr std::uint32_t AudioSoundVtable = 0x006EEEA4;
}
