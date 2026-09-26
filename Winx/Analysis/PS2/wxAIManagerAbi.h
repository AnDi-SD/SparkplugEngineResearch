#pragma once
#include "Analysis/PS2/SparkBaseAbi.h"
#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxAIManagerLayout final
    {
        sparkplug::evidence::ps2::spBaseObjectLayout base;
        std::uint32_t secondaryVtable; // 10
        std::uint32_t count;           // 14
        std::uint32_t sentinelPrevious, sentinelNext; // embedded header at 18
        std::uint32_t cursor20, cursor24;
        std::uint32_t counter28, counter2C;
        std::uint8_t triggered30, padding31[3];
        std::uint32_t counter34, counter38, field3C, respawnPoint;
        std::uint8_t pending44, padding45[3];
    };
    static_assert(sizeof(wxAIManagerLayout) == 0x48);
    static_assert(offsetof(wxAIManagerLayout, cursor24) == 0x24);
    static_assert(offsetof(wxAIManagerLayout, pending44) == 0x44);
    inline constexpr std::uint32_t wxAIManagerConstructor = 0x00224C60;
    inline constexpr std::uint32_t wxAIManagerFactory = 0x00224E70;
    inline constexpr std::uint32_t wxAIManagerVtable = 0x004920B0;
    inline constexpr std::uint32_t wxAIManagerSecondaryVtable = 0x004920D4;
    inline constexpr std::uint32_t wxAIManagerUpdate = 0x00224340;
}
