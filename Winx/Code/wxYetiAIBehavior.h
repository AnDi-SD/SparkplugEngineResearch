#pragma once

#include <array>
#include <cstdint>

// Captured value facade for two PC idle-time descriptor leaves.
class wxYetiAIBehavior
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x1C0>;
    enum class PropertyForAnalysis { MinIdleTime, MaxIdleTime };
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryIdleTimeRawDwords1B8And1BC };

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
