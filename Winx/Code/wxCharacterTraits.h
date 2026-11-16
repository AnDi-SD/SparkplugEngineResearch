#pragma once
#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    // Analytical value facade; this buffer is not a native C++ object.
    class wxCharacterTraits final
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x1A8>;
        enum class PropertyProfileForAnalysis { Unqualified, PcCapturedPrimaryRaw1A4 };
        enum class PropertyResultForAnalysis { Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted };

        static PropertyResultForAnalysis ReadValueOfFairyStarsRawForAnalysis(
            const CapturedPrefixForAnalysis& before, std::uint32_t& output,
            PropertyProfileForAnalysis profile = PropertyProfileForAnalysis::Unqualified) noexcept;
        static PropertyResultForAnalysis WriteValueOfFairyStarsRawForAnalysis(
            const CapturedPrefixForAnalysis& before, std::uint32_t value,
            CapturedPrefixForAnalysis& output,
            PropertyProfileForAnalysis profile = PropertyProfileForAnalysis::Unqualified) noexcept;
    };
}
