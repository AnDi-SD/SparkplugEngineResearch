#pragma once

#include <array>
#include <cstdint>

// Value facade for the PC Amplitude callbacks on a captured receiver.
class wxWandringNPCAIBehavior
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x1D0>;
    using AmplitudeReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryFiniteNormalOrZeroF32 };
    enum class ResultForAnalysis
    {
        Completed,
        UnqualifiedProfile,
        CapturedPrimaryNotAdmitted,
        CapturedAmplitudeNotAdmitted,
        ArgumentNotAdmitted,
    };

    struct ComposedAmplitudeForAnalysis
    {
        AmplitudeReturnForAnalysis before{};
        CapturedPrefixForAnalysis afterSet{};
        AmplitudeReturnForAnalysis after{};
    };

    static ResultForAnalysis ReadAmplitudeForAnalysis(const CapturedPrefixForAnalysis& before,
        AmplitudeReturnForAnalysis& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static ResultForAnalysis WriteAmplitudeBitsForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t argument, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static ResultForAnalysis ComposeAmplitudeForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t argument, ComposedAmplitudeForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
};
