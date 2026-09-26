#pragma once
#include <cstddef>
#include <cstdint>
namespace winx::evidence::pc
{
    struct wxAlphaManagerLayout final
    {
        std::uint8_t base[0x10];
        std::uint32_t secondaryVtable, effects[200], listUnknown, listSentinel, listCount, field340;
        std::uint8_t enabled, notified, padding[2];
        float from, to;
        std::uint32_t started, duration, material, field35C, recipient;
        float flashColor[3];
    };
    static_assert(sizeof(wxAlphaManagerLayout)==0x370);
    static_assert(offsetof(wxAlphaManagerLayout,effects)==0x14);
    static_assert(offsetof(wxAlphaManagerLayout,enabled)==0x344);
    inline constexpr std::uint32_t wxAlphaManagerConstructor=0x594B70, wxAlphaManagerFactory=0x594D00;
    inline constexpr std::uint32_t wxAlphaManagerVtable=0x702B64, wxAlphaManagerSingleton=0x75528C;
}
