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
};
