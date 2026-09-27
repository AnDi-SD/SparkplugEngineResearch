#pragma once
#include <cstddef>
#include <cstdint>
namespace winx::evidence::pc
{
    struct wxAudioEmitterLayout final
    {
        std::uint8_t wxEntity[0x124];
        std::uint32_t ownedResource;
        std::uint8_t nativeContainers[0x34];
        std::uint8_t flag5C, flag5D, unknown15E[2];
        std::uint8_t unknown160[0x20];
        std::uint32_t words[3];
        std::uint8_t tail[8];
    };
    static_assert(sizeof(wxAudioEmitterLayout) == 0x194);
    static_assert(offsetof(wxAudioEmitterLayout, ownedResource) == 0x124);
    static_assert(offsetof(wxAudioEmitterLayout, flag5C) == 0x15C);
    static_assert(offsetof(wxAudioEmitterLayout, words) == 0x180);
    inline constexpr std::uint32_t wxAudioEmitterConstructor = 0x587A50;
    inline constexpr std::uint32_t wxAudioEmitterFactory = 0x407B20;
    inline constexpr std::uint32_t wxAudioEmitterVtable = 0x701B94;
    inline constexpr std::uint32_t wxAudioEmitterTag = 0x586C60;
}
