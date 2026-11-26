#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxDispelTrigger
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x1C4>;

        // A true result admits a capture for the corresponding PC byte store.
        [[nodiscard]] static bool TrySetPcActiveByteForAnalysis(CapturedPrefixForAnalysis& state) noexcept;
        [[nodiscard]] static bool TryClearPcActiveByteForAnalysis(CapturedPrefixForAnalysis& state) noexcept;
    };
}
