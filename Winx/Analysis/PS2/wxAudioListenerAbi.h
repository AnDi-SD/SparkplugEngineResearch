#pragma once
#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxAudioListenerLayout final
    {
        std::uint8_t wxEntity[0x130];
        std::uint32_t listener;
        std::uint8_t nativeContainer[0x1C];
        std::uint8_t initialized;
        std::uint8_t unknown151[0xF];
    };
    static_assert(sizeof(wxAudioListenerLayout) == 0x160);
    static_assert(offsetof(wxAudioListenerLayout, listener) == 0x130);
    static_assert(offsetof(wxAudioListenerLayout, initialized) == 0x150);
    inline constexpr std::uint32_t wxAudioListenerConstructor = 0x3CEF70;
    inline constexpr std::uint32_t wxAudioListenerFactory = 0x3E6850;
    inline constexpr std::uint32_t wxAudioListenerVtable = 0x4954F0;
    inline constexpr std::uint32_t wxAudioListenerInitialize = 0x3CE4C8;
}
