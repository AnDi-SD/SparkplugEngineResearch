#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxTargetRegion
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x154>;

        // Applies the PC payload following a successful parent Copy.
        // The two captures are distinct and their source text has a zero
        // within the first 32 bytes of the copied range.
        [[nodiscard]] static bool TryCopyPcPayloadForAnalysis(const CapturedPrefixForAnalysis& source,
            CapturedPrefixForAnalysis& destination) noexcept;
    };
}
