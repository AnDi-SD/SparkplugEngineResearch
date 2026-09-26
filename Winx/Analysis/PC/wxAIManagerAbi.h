#pragma once
#include "Analysis/PC/SparkBaseAbi.h"
#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxAIManagerLayout final
    {
        sparkplug::evidence::pc::spBaseObjectLayout base;
        std::uint32_t secondaryVtable; // 10: singleton-related subobject
        std::uint32_t allocatorStorage; // 14: not initialized by constructor
        std::uint32_t sentinel;         // 18: allocated circular list header
        std::uint32_t count;            // 1C
        std::uint32_t cursor20, cursor24;
        std::uint32_t counter28, counter2C;
        std::uint8_t triggered30, padding31[3]; // padding not initialized
        std::uint32_t counter34, counter38, field3C, respawnPoint;
        std::uint8_t pending44, padding45[3];
    };
    static_assert(sizeof(wxAIManagerLayout) == 0x48);
    static_assert(offsetof(wxAIManagerLayout, cursor24) == 0x24);
    static_assert(offsetof(wxAIManagerLayout, pending44) == 0x44);
    inline constexpr std::uint32_t wxAIManagerConstructor = 0x005A5F90;
    inline constexpr std::uint32_t wxAIManagerFactory = 0x005A6100;
    inline constexpr std::uint32_t wxAIManagerVtable = 0x00706640;
    inline constexpr std::uint32_t wxAIManagerSecondaryVtable = 0x0070663C;
    inline constexpr std::uint32_t wxAIManagerSingleton = 0x00765BC0;
    inline constexpr std::uint32_t wxAIManagerUpdate = 0x005A5CF0;
}
