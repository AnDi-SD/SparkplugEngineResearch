#pragma once

#include <array>
#include <cstdint>

// Captured value facade for two PC descriptor leaves.
class wxStarEmitterTarget
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x15C>;
    enum class PropertyForAnalysis { BurstQty, BurstStarValue };
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryBurstRawDwords154And158 };

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
