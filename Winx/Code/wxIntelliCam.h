#pragma once
#include <array>
#include <cstdint>

// Supplied captured values only; this declares no native object layout.
class wxIntelliCam
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 1000>;
    using ScalarReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class ScalarPropertyForAnalysis { MouseRotationSpeed, JoystickRotationSpeed, DistanceAcceleration, DistanceStiffness, HomeElevation, ElevationAcceleration, ElevationStiffness, RotationAcceleration, RotationStiffness, YOffset };
    enum class ScalarProfileForAnalysis { Unqualified, PcCapturedPrimaryFiniteNormalOrZeroF32 };
    enum class ScalarResultForAnalysis
    {
        Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted,
        UnknownProperty, CapturedScalarNotAdmitted, ArgumentNotAdmitted
    };
    static ScalarResultForAnalysis ReadScalarForAnalysis(const CapturedPrefixForAnalysis& before,
        ScalarPropertyForAnalysis property, ScalarReturnForAnalysis& output,
        ScalarProfileForAnalysis profile = ScalarProfileForAnalysis::Unqualified) noexcept;
    static ScalarResultForAnalysis WriteScalarBitsForAnalysis(const CapturedPrefixForAnalysis& before,
        ScalarPropertyForAnalysis property, std::uint32_t rawArgument, CapturedPrefixForAnalysis& output,
        ScalarProfileForAnalysis profile = ScalarProfileForAnalysis::Unqualified) noexcept;
};
