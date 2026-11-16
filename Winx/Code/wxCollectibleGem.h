#pragma once

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction { class spNode; }

namespace winx::reconstruction
{
    class wxEntity;

    // Bounded facade for PC Gem slot+2C at00544C90. The supplied common
    // wxEntity, own bytes, record flag word and Nodes are borrowed analytical
    // bindings. This facade does not construct or own the native Gem graph.
    class wxCollectibleGem
    {
    public:
        static constexpr std::uint32_t ClassID = 0x27F540A1;
        using OwnBytesForAnalysis = std::array<std::uint8_t, 0x154 - 0x124>;

        // Common Entity host writes boundRecordFlags through its existing
        // WriteComputedFlagsForAnalysis callback. Bindings stay stable during
        // this call. The qualified base profile has a warm empty predicate
        // cache and field3C=7F7FFFFF. Node world caches are supplied unchanged.
        // Geometry uses finite coordinates and radius with abs(value)<=16384.
        // Unsupported input checks are our analysis contract. A geometry
        // rejection happens after the common Entity update and preserves
        // that update's bound flag word. Own bytes and Nodes stay unchanged.
        [[nodiscard]] static bool PredicateForAnalysis(
            wxEntity& entity,
            const OwnBytesForAnalysis& ownBytes,
            const std::uint32_t& boundRecordFlags,
            const sparkplug::reconstruction::spNode* otherNode,
            const sparkplug::reconstruction::spNode* ownerNode);
    };
}
