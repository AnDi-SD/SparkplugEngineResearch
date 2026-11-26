#pragma once
#include <array>
#include <cstdint>

// Captured values only; this facade does not declare the native object layout.
class wxUpsieDaisyGuardAIBehavior
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x188>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDword184 };
    struct ComposedGuardNumForAnalysis
    {
        std::uint32_t beforeRaw{};
        CapturedPrefixForAnalysis afterSet{};
        std::uint32_t afterRaw{};
    };
    static bool ReadGuardNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteGuardNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawBits, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeGuardNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawBits, ComposedGuardNumForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);

    using ScalarPrefixForAnalysis = std::array<std::uint8_t, 0x19C>;
    using ScalarReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class ScalarPropertyForAnalysis { WalkSpeed, TurnSpeed, VisionCos, VisionLengthSquared };
    enum class ScalarProfileForAnalysis { Unqualified, PcCapturedPrimaryFiniteNormalOrZeroF32 };
    enum class ScalarResultForAnalysis
    {
        Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted,
        UnknownProperty, CapturedScalarNotAdmitted, ArgumentNotAdmitted
    };
    static ScalarResultForAnalysis ReadScalarForAnalysis(const ScalarPrefixForAnalysis& before,
        ScalarPropertyForAnalysis property, ScalarReturnForAnalysis& output,
        ScalarProfileForAnalysis profile = ScalarProfileForAnalysis::Unqualified) noexcept;
    static ScalarResultForAnalysis WriteScalarBitsForAnalysis(const ScalarPrefixForAnalysis& before,
        ScalarPropertyForAnalysis property, std::uint32_t rawArgument, ScalarPrefixForAnalysis& output,
        ScalarProfileForAnalysis profile = ScalarProfileForAnalysis::Unqualified) noexcept;

    using BytePrefixForAnalysis = std::array<std::uint8_t, 0x1D6>;
    enum class BytePropertyForAnalysis { WaitForSignal, FollowFloor };
    enum class ByteProfileForAnalysis { Unqualified, PcCapturedPrimaryRawBytes1D4And1D5 };

    // The captured value API exposes the low byte selected by the PC callbacks.
    static bool ReadBytePropertyForAnalysis(const BytePrefixForAnalysis& before,
        BytePropertyForAnalysis property, std::uint8_t& output,
        ByteProfileForAnalysis profile = ByteProfileForAnalysis::Unqualified) noexcept;
    static bool WriteLowBytePropertyForAnalysis(const BytePrefixForAnalysis& before,
        BytePropertyForAnalysis property, std::uint32_t rawArgumentWord, BytePrefixForAnalysis& output,
        ByteProfileForAnalysis profile = ByteProfileForAnalysis::Unqualified) noexcept;
};
