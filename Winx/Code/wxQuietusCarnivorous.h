#pragma once
#include <array>
#include <cstdint>

// Captured values only; this facade does not declare the native object layout.
class wxQuietusCarnivorous
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x168>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDword164 };
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
};
