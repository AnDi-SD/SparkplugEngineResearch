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
};
