#pragma once

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spDXEnvironmentMapLayer
    {
      public:
        static constexpr std::uint32_t ClassID = 0x3F424FCD;
        using CapturedPcPrefixForAnalysis = std::array<std::uint8_t, 0x18>;

        // Both captures start at separate PC owners and hold their observed primary table words.
        // Reports admission to the own-payload copy. Refusal preserves the destination.
        [[nodiscard]] static bool TryCopyPcPayloadForAnalysis(const CapturedPcPrefixForAnalysis& source,
            CapturedPcPrefixForAnalysis& destination) noexcept;
    };
}
