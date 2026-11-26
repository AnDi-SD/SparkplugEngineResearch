#pragma once

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spMaterialMovieTexture
    {
      public:
        static constexpr std::uint32_t ClassID = 0x6A24474A;
        using CapturedPcMaterialForAnalysis = std::array<std::uint8_t, 0x6C>;
        using OpaqueResourcePrefixForAnalysis = std::array<std::uint8_t, 0x1C>;

        struct BorrowedResourceForAnalysis
        {
            std::uint32_t capturedAddress;
            const OpaqueResourcePrefixForAnalysis& prefix;
        };

        // Match the captured material's PC identity and resource address first.
        [[nodiscard]] static bool ReadPcResourceWordForAnalysis(const CapturedPcMaterialForAnalysis& material,
            const BorrowedResourceForAnalysis& resource, std::uint32_t& output) noexcept;
    };
} // namespace sparkplug::reconstruction
