#pragma once

#include <array>
#include <cstdint>

// Captured value facade for the PC Trigger Radius and Movement Speed properties.
class wxIceBatStateMachine
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x234>;
    using ScalarReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class PropertyForAnalysis { TriggerRadius, MovementSpeed };
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryScalarDwords22CAnd230 };
    enum class ResultForAnalysis
    {
        Completed,
        UnqualifiedProfile,
        CapturedPrimaryNotAdmitted,
        UnknownProperty,
        CapturedScalarNotAdmitted,
        ArgumentNotAdmitted,
    };

    struct ComposedScalarForAnalysis
    {
        ScalarReturnForAnalysis before{};
        CapturedPrefixForAnalysis afterSet{};
        ScalarReturnForAnalysis after{};
    };

    static ResultForAnalysis ReadScalarForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, ScalarReturnForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static ResultForAnalysis WriteScalarBitsForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t argument, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static ResultForAnalysis ComposeScalarForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t argument, ComposedScalarForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
};
