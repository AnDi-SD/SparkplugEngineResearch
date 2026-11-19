#pragma once
#include <array>
#include <cstdint>

// Captured values only. This facade does not declare the native object layout.
class wxFallingRock
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x18C>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDwords170180184188 };
    enum class PropertyForAnalysis { WarningDeltaRaw188, FallingDurationRaw184, FallingIntervalRaw180, InflictedDamageRaw170 };
    struct ComposedPropertiesForAnalysis
    {
        // Publisher order; each pair is GET before SET and GET after SET.
        std::array<std::uint32_t, 8> rawGetterResults{};
        std::array<CapturedPrefixForAnalysis, 4> afterSet{};
    };
    static bool ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t rawBits, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeRawPropertiesForAnalysis(const CapturedPrefixForAnalysis& before,
        const std::array<std::uint32_t, 4>& rawArguments, ComposedPropertiesForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);

    using ScalarReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class ScalarPropertyForAnalysis
    {
        GravityAcceleration, RotationSpeed, MaximumHeightDelta,
        SquaredProximityTrigger, Elasticity, Radius
    };
    enum class ScalarProfileForAnalysis { Unqualified, PcCapturedPrimaryFiniteNormalOrZeroF32 };
    enum class ScalarResultForAnalysis
    {
        Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted,
        UnknownProperty, CapturedScalarNotAdmitted, ArgumentNotAdmitted
    };

    // The scalar profile admits finite normal binary32 values and signed zero
    // at its six selected fields and in setter arguments. A getter supplies
    // the physical ten-byte FLD value. Refusal preserves every output byte.
    static ScalarResultForAnalysis ReadScalarForAnalysis(const CapturedPrefixForAnalysis& before,
        ScalarPropertyForAnalysis property, ScalarReturnForAnalysis& output,
        ScalarProfileForAnalysis profile = ScalarProfileForAnalysis::Unqualified) noexcept;
    static ScalarResultForAnalysis WriteScalarBitsForAnalysis(const CapturedPrefixForAnalysis& before,
        ScalarPropertyForAnalysis property, std::uint32_t rawArgument, CapturedPrefixForAnalysis& output,
        ScalarProfileForAnalysis profile = ScalarProfileForAnalysis::Unqualified) noexcept;
};
