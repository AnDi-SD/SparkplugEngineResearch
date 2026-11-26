#pragma once

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spDXSceneGraphOptimizer
    {
    public:
        static constexpr std::uint32_t ClassID = 0x7E120EC3;
        using CapturedOwnerForAnalysis = std::array<std::uint8_t, 0x54>;

        struct CapturedGroupSentinelForAnalysis
        {
            std::uint32_t capturedAddress = 0;
            std::array<std::uint8_t, 0x20> bytes{};
        };

        struct CapturedBatchSentinelForAnalysis
        {
            std::uint32_t capturedAddress = 0;
            std::array<std::uint8_t, 0x0C> bytes{};
        };

        // The captures supply the two distinct empty heads referenced by the owner.
        [[nodiscard]] static bool TryReadPcEmptyOnNodeForAnalysis(const CapturedOwnerForAnalysis& owner,
            const CapturedGroupSentinelForAnalysis& group, const CapturedBatchSentinelForAnalysis& batch,
            std::uint8_t& output) noexcept;
    };
}
