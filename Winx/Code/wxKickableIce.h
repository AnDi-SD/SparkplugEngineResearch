#pragma once

#include <array>
#include <cstdint>

// Captured value facade for three PC descriptor pairs.
class wxKickableIce
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x1FC>;
    enum class PropertyForAnalysis { IceId, CollectibleType, FairyStarsValue };
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryIceRawDwords };

    struct ComposedRawPropertyForAnalysis
    {
        std::uint32_t beforeRaw = 0;
        CapturedPrefixForAnalysis afterSet{};
        std::uint32_t afterRaw = 0;
    };

    static bool ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t rawArgument, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool ComposeRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t rawArgument, ComposedRawPropertyForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;

    using ImpulseRangeReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class ImpulseRangeProfileForAnalysis { Unqualified, PcCapturedPrimaryImpulseRangeF32 };
    enum class ImpulseRangeResultForAnalysis
    {
        Completed,
        UnqualifiedProfile,
        CapturedPrimaryNotAdmitted,
        CapturedRangeNotAdmitted,
        ArgumentNotAdmitted,
    };

    struct ComposedImpulseRangeForAnalysis
    {
        ImpulseRangeReturnForAnalysis before{};
        CapturedPrefixForAnalysis afterSet{};
        ImpulseRangeReturnForAnalysis after{};
    };

    static ImpulseRangeResultForAnalysis ReadImpulseRangeForAnalysis(const CapturedPrefixForAnalysis& before,
        ImpulseRangeReturnForAnalysis& output,
        ImpulseRangeProfileForAnalysis profile = ImpulseRangeProfileForAnalysis::Unqualified) noexcept;
    static ImpulseRangeResultForAnalysis WriteImpulseRangeBitsForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t argument, CapturedPrefixForAnalysis& output,
        ImpulseRangeProfileForAnalysis profile = ImpulseRangeProfileForAnalysis::Unqualified) noexcept;
    static ImpulseRangeResultForAnalysis ComposeImpulseRangeForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t argument, ComposedImpulseRangeForAnalysis& output,
        ImpulseRangeProfileForAnalysis profile = ImpulseRangeProfileForAnalysis::Unqualified) noexcept;
};
