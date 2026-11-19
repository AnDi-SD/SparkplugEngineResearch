#pragma once
#include <array>
#include <cstdint>

// Captured values only; this facade does not declare the native object layout.
class wxSwampGazBubble
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x150>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDword14C, PcCapturedPrimaryRawByte144 };
    struct ComposedDamageForAnalysis
    {
        std::uint32_t beforeRaw{};
        CapturedPrefixForAnalysis afterSet{};
        std::uint32_t afterRaw{};
    };
    static bool ReadDamageInflictedRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteDamageInflictedRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawBits, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeDamageInflictedRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawBits, ComposedDamageForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    // Raw byte value callback facade. Native upper EAX/return ABI are not exposed.
    struct ComposedGreenByteForAnalysis
    {
        std::uint8_t beforeRaw{};
        CapturedPrefixForAnalysis afterSet{};
        std::uint8_t afterRaw{};
    };
    static bool ReadGreenByteForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint8_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteGreenLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawArgumentWord, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeGreenLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawArgumentWord, ComposedGreenByteForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    // Separate supplied scalar prefix; old RAW API retains its 336B type.
    using ScalarCapturedPrefixForAnalysis = std::array<std::uint8_t, 0x160>;
    using ScalarReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class ScalarPropertyForAnalysis
    {
        MaximumExplosionRadius, MinimumExplosionRadius, MaximumInflationSpeed, MinimumInflationSpeed, DamageRadius
    };
    enum class ScalarProfileForAnalysis { Unqualified, PcCapturedPrimaryFiniteNormalOrZeroF32 };
    enum class ScalarResultForAnalysis
    {
        Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted,
        UnknownProperty, CapturedScalarNotAdmitted, ArgumentNotAdmitted
    };

    // The scalar profile admits finite normal binary32 values and signed zero
    // at its five selected fields and in setter arguments. A getter supplies
    // the physical ten-byte FLD value. Refusal preserves every output byte.
    static ScalarResultForAnalysis ReadScalarForAnalysis(const ScalarCapturedPrefixForAnalysis& before,
        ScalarPropertyForAnalysis property, ScalarReturnForAnalysis& output,
        ScalarProfileForAnalysis profile = ScalarProfileForAnalysis::Unqualified) noexcept;
    static ScalarResultForAnalysis WriteScalarBitsForAnalysis(const ScalarCapturedPrefixForAnalysis& before,
        ScalarPropertyForAnalysis property, std::uint32_t rawArgument, ScalarCapturedPrefixForAnalysis& output,
        ScalarProfileForAnalysis profile = ScalarProfileForAnalysis::Unqualified) noexcept;
};
