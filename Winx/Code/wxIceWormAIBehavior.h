#pragma once

#include <array>
#include <cstdint>

// Captured value facade for two PC hole descriptor leaves.
class wxIceWormAIBehavior
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x1BC>;
    enum class PropertyForAnalysis { HolesFrequency, HolesOffset };
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryHolesRawDwords1B4And1B8 };

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
