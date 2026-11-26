#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxColourfulFlower
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x150>;

        // Applies the PC field transfers reached after a successful parent Copy.
        // The result reports whether the supplied pair of captures was accepted.
        [[nodiscard]] static bool TryCopyPcPayloadForAnalysis(const CapturedPrefixForAnalysis& source,
            CapturedPrefixForAnalysis& destination) noexcept;
    };
}
