#pragma once
#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    // Captured-value analysis facade for one own-published PC Scale callback pair.
    // The prefix and returned payload are values, not native C++ objects.
    class wxSideQuestGivenObject final
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x134>;
        using ExtendedReturnForAnalysis = std::array<std::uint8_t, 10>;
        enum class PropertyForAnalysis { Scale };
        enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryFiniteNormalOrZeroF32 };
        enum class ResultForAnalysis
        {
            Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted,
            UnknownProperty, CapturedScalarNotAdmitted, ArgumentNotAdmitted
        };

        // Qualified captured fields and setter arguments are finite binary32
        // normals or signed zero. The getter returns the exact ten-byte value
        // loaded by FLD, including zero sign; native stack/ABI is not emulated.
        // All output stays unchanged on refusal. Prefix input/output may alias.
        static ResultForAnalysis ReadScalarForAnalysis(const CapturedPrefixForAnalysis& before,
            PropertyForAnalysis property, ExtendedReturnForAnalysis& output,
            ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
        static ResultForAnalysis WriteScalarBitsForAnalysis(const CapturedPrefixForAnalysis& before,
            PropertyForAnalysis property, std::uint32_t rawArgument, CapturedPrefixForAnalysis& output,
            ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    };
}
