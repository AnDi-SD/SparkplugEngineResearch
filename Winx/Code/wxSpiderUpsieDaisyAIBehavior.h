#pragma once
#include <array>
#include <cstdint>

// Captured values only; this facade does not declare the native object layout.
class wxSpiderUpsieDaisyAIBehavior
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x138>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDword134 };
    struct ComposedSpiderNumForAnalysis
    {
        std::uint32_t beforeRaw{};
        CapturedPrefixForAnalysis afterSet{};
        std::uint32_t afterRaw{};
    };
    static bool ReadSpiderNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteSpiderNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawBits, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeSpiderNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawBits, ComposedSpiderNumForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);

    using SpeedPrefixForAnalysis = std::array<std::uint8_t, 0x144>;
    using SpeedReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class SpeedPropertyForAnalysis { WalkSpeed, TurnSpeed };
    enum class SpeedProfileForAnalysis { Unqualified, PcCapturedPrimaryFiniteNormalOrZeroF32 };
    enum class SpeedResultForAnalysis
    {
        Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted,
        UnknownProperty, CapturedSpeedNotAdmitted, ArgumentNotAdmitted
    };
    static SpeedResultForAnalysis ReadSpeedForAnalysis(const SpeedPrefixForAnalysis& before,
        SpeedPropertyForAnalysis property, SpeedReturnForAnalysis& output,
        SpeedProfileForAnalysis profile = SpeedProfileForAnalysis::Unqualified) noexcept;
    static SpeedResultForAnalysis WriteSpeedBitsForAnalysis(const SpeedPrefixForAnalysis& before,
        SpeedPropertyForAnalysis property, std::uint32_t rawArgument, SpeedPrefixForAnalysis& output,
        SpeedProfileForAnalysis profile = SpeedProfileForAnalysis::Unqualified) noexcept;
};
