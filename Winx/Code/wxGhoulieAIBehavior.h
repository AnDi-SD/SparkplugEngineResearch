#pragma once

#include <array>
#include <cstdint>

// Value facade for two raw bytes of a captured PC Ghoulie behavior.
class wxGhoulieAIBehavior
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x1C6>;
    enum class PropertyForAnalysis { IsInGardenia01, JumpingGhoulie };
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawBytes1C4And1C5 };

    struct ComposedRawByteForAnalysis
    {
        std::uint8_t beforeRaw = 0;
        CapturedPrefixForAnalysis afterSet{};
        std::uint8_t afterRaw = 0;
    };

    static bool ReadRawByteForAnalysis(const CapturedPrefixForAnalysis& before, PropertyForAnalysis property,
        std::uint8_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool WriteLowByteForAnalysis(const CapturedPrefixForAnalysis& before, PropertyForAnalysis property,
        std::uint32_t argument, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool ComposeLowByteForAnalysis(const CapturedPrefixForAnalysis& before, PropertyForAnalysis property,
        std::uint32_t argument, ComposedRawByteForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
};
