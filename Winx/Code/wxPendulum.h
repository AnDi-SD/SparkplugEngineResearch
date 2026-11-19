#pragma once
#include <array>
#include <cstdint>

// Captured value facade. This does not declare the native object's full layout.
class wxPendulum
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x12C>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDwords084128 };
    enum class PropertyForAnalysis { PeriodRaw128, DamageRaw084 };
    struct ComposedPropertiesForAnalysis
    {
        std::array<std::uint32_t, 6> rawGetterResults{};
        CapturedPrefixForAnalysis afterPeriodSet{};
        CapturedPrefixForAnalysis afterDamageSet{};
    };
    static bool ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t rawBits, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeRawPropertiesForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawPeriod, std::uint32_t rawDamage, ComposedPropertiesForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);

    // Separate scalar extent: the original RAW300 facade remains unchanged.
    using CapturedScalarPrefixForAnalysis = std::array<std::uint8_t, 0x154>;

    using ScalarReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class ScalarPropertyForAnalysis
    {
        MaximumAngle, InitialOffset
    };
    enum class ScalarProfileForAnalysis { Unqualified, PcCapturedPrimaryFiniteNormalOrZeroF32 };
    enum class ScalarResultForAnalysis
    {
        Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted,
        UnknownProperty, CapturedScalarNotAdmitted, ArgumentNotAdmitted
    };

    // The scalar profile admits finite normal binary32 values and signed zero
    // at its two selected fields and in setter arguments. A getter supplies
    // the physical ten-byte FLD value. Refusal preserves every output byte.
    static ScalarResultForAnalysis ReadScalarForAnalysis(const CapturedScalarPrefixForAnalysis& before,
        ScalarPropertyForAnalysis property, ScalarReturnForAnalysis& output,
        ScalarProfileForAnalysis profile = ScalarProfileForAnalysis::Unqualified) noexcept;
    static ScalarResultForAnalysis WriteScalarBitsForAnalysis(const CapturedScalarPrefixForAnalysis& before,
        ScalarPropertyForAnalysis property, std::uint32_t rawArgument, CapturedScalarPrefixForAnalysis& output,
        ScalarProfileForAnalysis profile = ScalarProfileForAnalysis::Unqualified) noexcept;
};
