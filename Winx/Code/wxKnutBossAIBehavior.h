#pragma once

#include <array>
#include <cstdint>

// Captured value facade for four PC raw DWORD descriptor leaves.
class wxKnutBossAIBehavior
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x1D0>;
    enum class PropertyForAnalysis { HitsForLosingGlasses, HitsForFalling, MinTimeBetweenAttacks, MaxTimeBetweenAttacks };
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDwords1B0To1CC };

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
};
