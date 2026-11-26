#pragma once

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spDXKeyboard
    {
    public:
        static constexpr std::uint32_t ClassID = 0x3A87729D;
        using CapturedPcPrefixForAnalysis = std::array<std::uint8_t, 0xA2C>;

        // The prefix starts at the keyboard owner. The result is its current-key AL.
        // Admission requires both captured identities and a known raw key.
        [[nodiscard]] static bool TryReadPcCurrentKeyForAnalysis(const CapturedPcPrefixForAnalysis& prefix,
            std::uint32_t rawKey, std::uint8_t& output) noexcept;

        // PC secondary slot +0x08 returns the changed-key byte comparison.
        [[nodiscard]] static bool TryReadPcChangedKeyForAnalysis(const CapturedPcPrefixForAnalysis& prefix,
            std::uint32_t rawKey, std::uint8_t& output) noexcept;
    };
}
