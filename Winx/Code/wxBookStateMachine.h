#pragma once

#include <array>
#include <cstdint>

// Value facade for the PC Scale callbacks on a captured receiver.
class wxBookStateMachine
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x228>;
    using ScaleReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryFiniteNormalOrZeroF32 };
    enum class ResultForAnalysis
    {
        Completed,
        UnqualifiedProfile,
        CapturedPrimaryNotAdmitted,
        CapturedScaleNotAdmitted,
        ArgumentNotAdmitted,
    };

    struct ComposedScaleForAnalysis
    {
        ScaleReturnForAnalysis before{};
        CapturedPrefixForAnalysis afterSet{};
        ScaleReturnForAnalysis after{};
    };

    static ResultForAnalysis ReadScaleForAnalysis(const CapturedPrefixForAnalysis& before,
        ScaleReturnForAnalysis& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static ResultForAnalysis WriteScaleBitsForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t argument, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static ResultForAnalysis ComposeScaleForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t argument, ComposedScaleForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
};
