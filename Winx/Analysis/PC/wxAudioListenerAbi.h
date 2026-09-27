#pragma once
#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxAudioListenerLayout final
    {
        std::uint8_t wxEntity[0x124];
        std::uint32_t listener;
        std::uint8_t nativeContainer[0x1C];
        std::uint8_t initialized;
        std::uint8_t unknown145[3];
    };
    static_assert(sizeof(wxAudioListenerLayout) == 0x148);
    static_assert(offsetof(wxAudioListenerLayout, listener) == 0x124);
    static_assert(offsetof(wxAudioListenerLayout, initialized) == 0x144);
    inline constexpr std::uint32_t wxAudioListenerConstructor = 0x587D80;
    inline constexpr std::uint32_t wxAudioListenerFactory = 0x407B80;
    inline constexpr std::uint32_t wxAudioListenerVtable = 0x701BE4;
    inline constexpr std::uint32_t wxAudioListenerInitialize = 0x587EE0;
}
