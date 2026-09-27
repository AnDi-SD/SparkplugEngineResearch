#pragma once
#include <cstddef>
#include <cstdint>
namespace winx::evidence::ps2
{
    struct wxAudioEmitterLayout final
    {
        std::uint8_t wxEntity[0x130];
        std::uint32_t ownedResource;
        std::uint8_t nativeContainers[0x40];
        std::uint8_t flag174, flag175, unknown176[0x1A];
        std::uint32_t words[3];
        std::uint8_t tail[0x14];
    };
    static_assert(sizeof(wxAudioEmitterLayout) == 0x1B0);
    static_assert(offsetof(wxAudioEmitterLayout, ownedResource) == 0x130);
    static_assert(offsetof(wxAudioEmitterLayout, flag174) == 0x174);
    static_assert(offsetof(wxAudioEmitterLayout, words) == 0x190);
    inline constexpr std::uint32_t wxAudioEmitterConstructor = 0x3CAC60;
    inline constexpr std::uint32_t wxAudioEmitterFactory = 0x3E6950;
    inline constexpr std::uint32_t wxAudioEmitterVtable = 0x495530;
    inline constexpr std::uint32_t wxAudioEmitterTag = 0x3C7DE0;
}
