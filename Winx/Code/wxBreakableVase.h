#pragma once
#include <array>
#include <cstdint>

// Captured values only; this facade does not declare the native object layout.
class wxBreakableVase
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x180>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDword17C };
    struct ComposedValueForAnalysis
    {
        std::uint32_t beforeRaw{};
        CapturedPrefixForAnalysis afterSet{};
        std::uint32_t afterRaw{};
    };
    static bool ReadFairyStarsValueRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteFairyStarsValueRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawBits, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeFairyStarsValueRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawBits, ComposedValueForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
};
