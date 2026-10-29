#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    struct spAudioBankLayout final
    {
        std::array<std::byte,0x10> base;
        std::uint32_t identifier10;
        std::uint32_t rawWord14;
        std::array<std::byte,4> unassigned18;
        std::uint32_t entriesBegin1C;
        std::uint32_t entriesEnd20;
        std::uint32_t entriesLimit24;
    };
    static_assert(sizeof(spAudioBankLayout)==0x28);
    static_assert(offsetof(spAudioBankLayout,entriesBegin1C)==0x1C);
    inline constexpr std::uint32_t spAudioBankConstructor=0x456CC0;
    inline constexpr std::uint32_t spAudioBankDestructor=0x49E730;
    inline constexpr std::uint32_t spAudioBankTable=0x6EED30;
    inline constexpr std::uint32_t spDXAudioBankFactory=0x4C7450;
    inline constexpr std::uint32_t spDXAudioBankClone=0x4C74C0;
    inline constexpr std::uint32_t spDXAudioBankDelete=0x4C7510;
    inline constexpr std::uint32_t spDXAudioBankTable=0x6F2B00;
}
