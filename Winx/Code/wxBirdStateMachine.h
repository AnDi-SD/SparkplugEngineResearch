#pragma once

#include <array>
#include <cstdint>

// Captured value facade for the PC Movement Radius and Safety Radius callbacks.
class wxBirdStateMachine
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x264>;
    using RadiusReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class PropertyForAnalysis { MovementRadius, SafetyRadius };
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRadiiDwords25CAnd260 };
    enum class ResultForAnalysis
    {
        Completed,
        UnqualifiedProfile,
        CapturedPrimaryNotAdmitted,
        UnknownProperty,
        CapturedRadiusNotAdmitted,
        ArgumentNotAdmitted,
    };

    struct ComposedRadiusForAnalysis
    {
        RadiusReturnForAnalysis before{};
        CapturedPrefixForAnalysis afterSet{};
        RadiusReturnForAnalysis after{};
    };

    static ResultForAnalysis ReadRadiusForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, RadiusReturnForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static ResultForAnalysis WriteRadiusBitsForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t argument, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static ResultForAnalysis ComposeRadiusForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t argument, ComposedRadiusForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
};
