#pragma once

// Observed PC32-bit allocation. Portable class storage is independent.
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    struct spSubtitleTrackObservedLayout final
    {
        std::array<std::uint8_t, 0x14> namedObjectBase;
        std::uint8_t opaqueByte;
        std::array<std::uint8_t, 3> untouchedPadding;
        std::uint32_t blob, recordCount, records, tableCount, selectedTable, tables;
    };
    static_assert(sizeof(spSubtitleTrackObservedLayout) == 0x30);
    static_assert(offsetof(spSubtitleTrackObservedLayout, opaqueByte) == 0x14);
    static_assert(offsetof(spSubtitleTrackObservedLayout, blob) == 0x18);
    static_assert(offsetof(spSubtitleTrackObservedLayout, recordCount) == 0x1C);
    static_assert(offsetof(spSubtitleTrackObservedLayout, records) == 0x20);
    static_assert(offsetof(spSubtitleTrackObservedLayout, tableCount) == 0x24);
    static_assert(offsetof(spSubtitleTrackObservedLayout, selectedTable) == 0x28);
    static_assert(offsetof(spSubtitleTrackObservedLayout, tables) == 0x2C);
    inline constexpr std::uint32_t spSubtitleTrackFactory = 0x6019A0;
    inline constexpr std::uint32_t spSubtitleTrackLoad = 0x601790;
    inline constexpr std::uint32_t spSubtitleTrackLookup = 0x601750;
    inline constexpr std::uint32_t spSubtitleTrackClear = 0x601920;
}
