#pragma once
#include <array>
#include <cstdint>

// Captured values only; this facade does not declare the native object layout.
class wxSwampGazBubble
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x150>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDword14C };
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
};
