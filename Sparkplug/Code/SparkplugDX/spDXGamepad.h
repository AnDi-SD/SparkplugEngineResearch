#pragma once

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spDXGamepad
    {
      public:
        static constexpr std::uint32_t ClassID = 0x63663F48;
        using CapturedPcPrefixForAnalysis = std::array<std::uint8_t, 0xC0>;

        // The prefix begins at the owner, including the interface at +0x14.
        // Returns admission. A refused prefix leaves the output intact.
        [[nodiscard]] static bool TryReadPcCachedValueForAnalysis(const CapturedPcPrefixForAnalysis& prefix,
            std::uint32_t selector, std::uint32_t& output) noexcept;
    };
} // namespace sparkplug::reconstruction
