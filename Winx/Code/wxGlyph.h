#pragma once
#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    // Captured-value facade. This class does not declare the native object layout.
    class wxGlyph final
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x160>;
        using ExtendedReturnForAnalysis = std::array<std::uint8_t, 10>;
        enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryFacingFiniteNormalOrZeroF32NoOpSetter };
        enum class ResultForAnalysis { Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted, CapturedFacingNotAdmitted };
        struct ComposedFacingForAnalysis
        {
            ExtendedReturnForAnalysis before{}, after{};
            CapturedPrefixForAnalysis afterNoOp{};
        };
        static ResultForAnalysis ReadFacingForAnalysis(const CapturedPrefixForAnalysis& before,
            ExtendedReturnForAnalysis& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
        // The original RET4 setter reads neither argument nor receiver data and writes nothing.
        // Our value admission checks profile/primary, then copies the prefix unchanged.
        // Any raw argument and any facing bits are accepted here; outputs may alias inputs.
        static ResultForAnalysis ApplyFacingNoOpForAnalysis(const CapturedPrefixForAnalysis& before,
            std::uint32_t unusedRawArgument, CapturedPrefixForAnalysis& output,
            ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
        static ResultForAnalysis ComposeFacingForAnalysis(const CapturedPrefixForAnalysis& before,
            std::uint32_t unusedRawArgument, ComposedFacingForAnalysis& output,
            ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    };
}
