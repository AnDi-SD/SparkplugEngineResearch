#pragma once
#include <array>
#include <cstdint>

// Captured values only; this facade does not declare the native object layout.
class wxEyeBallSentry
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x148>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDword144 };
    struct ComposedDamageForAnalysis
    {
        std::uint32_t beforeRaw{};
        CapturedPrefixForAnalysis afterSet{};
        std::uint32_t afterRaw{};
    };
    static bool ReadDamageRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteDamageRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawBits, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeDamageRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawBits, ComposedDamageForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);

    using ScalarReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class ScalarPropertyForAnalysis
    {
        AngleMaximum, AngleMinimum, OscillationPeriod
    };
    enum class ScalarProfileForAnalysis { Unqualified, PcCapturedPrimaryFiniteNormalOrZeroF32 };
    enum class ScalarResultForAnalysis
    {
        Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted,
        UnknownProperty, CapturedScalarNotAdmitted, ArgumentNotAdmitted
    };

    // The scalar profile admits finite normal binary32 values and signed zero
    // at its three selected fields and in setter arguments. A getter supplies
    // the physical ten-byte FLD value. Refusal preserves every output byte.
    static ScalarResultForAnalysis ReadScalarForAnalysis(const CapturedPrefixForAnalysis& before,
        ScalarPropertyForAnalysis property, ScalarReturnForAnalysis& output,
        ScalarProfileForAnalysis profile = ScalarProfileForAnalysis::Unqualified) noexcept;
    static ScalarResultForAnalysis WriteScalarBitsForAnalysis(const CapturedPrefixForAnalysis& before,
        ScalarPropertyForAnalysis property, std::uint32_t rawArgument, CapturedPrefixForAnalysis& output,
        ScalarProfileForAnalysis profile = ScalarProfileForAnalysis::Unqualified) noexcept;
};
