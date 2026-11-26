#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxBush
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x18C>;

        // Applies the PC text payload reached after a successful parent Copy.
        // Admission requires two distinct typed captures and a zero in each source window.
        [[nodiscard]] static bool TryCopyPcPayloadForAnalysis(const CapturedPrefixForAnalysis& source,
            CapturedPrefixForAnalysis& destination) noexcept;
    };
}
