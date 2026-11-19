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
};
