#pragma once

#include <cstdint>

namespace sparkplug::reconstruction
{
    // Captured PC initializer for an existing, coherent empty query map.
    // This representation does not own a device or model native layout.
    class spPCLensFlareManager
    {
    public:
        static constexpr std::uint32_t ClassID = 0x4838786B;

        struct CapturedEmptyMapForAnalysis
        {
            std::uint32_t sentinel = 0;
            std::uint32_t first = 0;
            std::uint32_t root = 0;
            std::uint32_t last = 0;
            std::uint32_t count = 0;
            std::uint8_t sentinelIsNil = 0;
        };

        struct CapturedStateForAnalysis
        {
            std::uint32_t primary = 0;
            CapturedEmptyMapForAnalysis map;
            std::uint32_t outstandingQueries = 0;
            std::uint32_t borrowedDevice = 0;
            std::uint8_t enabled = 0;
            std::uint8_t capability = 0;
        };

        // Synchronous status from a capability query with null object output.
        // The device address is a borrowed identity and never dereferenced here.
        using QueryCapabilityForAnalysis = std::uint32_t (*)(void* context,
            std::uint32_t borrowedDevice, std::uint32_t queryType) noexcept;

        [[nodiscard]] static bool InitExistingEmptyMapForAnalysis(
            CapturedStateForAnalysis& state, QueryCapabilityForAnalysis query, void* context) noexcept;
    };
}
