#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxGiantBoulderTrigger
    {
      public:
        static constexpr std::uint32_t ClassID = 0x55E957F4;

        // Captured XYZ from each borrowed point's PC world-position cache.
        using PointForAnalysis = std::array<float, 3>;

        // The admitted inputs are finite normal floats or signed zeros with
        // magnitudes up to 16384. Refusal leaves the result intact.
        [[nodiscard]] static bool TryEvaluatePcProximityForAnalysis(const PointForAnalysis& first,
            const PointForAnalysis& second, bool& inside) noexcept;
    };
} // namespace winx::reconstruction
