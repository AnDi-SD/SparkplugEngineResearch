#pragma once
#include <array>
#include <cstdint>

// Captured value facade. This does not declare the native object's full layout.
class wxDamageProxy
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x130>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDwords12412C };
    enum class PropertyForAnalysis { DamagePeriodRaw12C, DamagePointsRaw124 };
    struct ComposedPropertiesForAnalysis
    {
        std::array<std::uint32_t, 6> rawGetterResults{};
        CapturedPrefixForAnalysis afterPeriodSet{};
        CapturedPrefixForAnalysis afterPointsSet{};
    };
    static bool ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t rawBits, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeRawPropertiesForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawPeriod, std::uint32_t rawPoints, ComposedPropertiesForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
};
