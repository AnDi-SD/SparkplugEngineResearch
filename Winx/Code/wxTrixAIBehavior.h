#pragma once

#include <array>
#include <cstdint>

// Captured value facade for four PC raw DWORD descriptor leaves.
class wxTrixAIBehavior
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x1C8>;
    enum class PropertyForAnalysis { MinIdleTime, MaxIdleTime, IceShieldTime, IceShieldHitCount };
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDwords1B8To1C4 };

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
