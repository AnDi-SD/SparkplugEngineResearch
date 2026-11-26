#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxSpinningStatuesPuzzle
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x15C>;

        // Applies the PC transfers reached after a successful parent Copy.
        // The result reports whether the supplied captures were accepted.
        [[nodiscard]] static bool TryCopyPcPayloadForAnalysis(const CapturedPrefixForAnalysis& source,
            CapturedPrefixForAnalysis& destination) noexcept;
    };
}
