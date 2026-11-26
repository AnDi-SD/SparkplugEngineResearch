#pragma once

#include <array>
#include <cstdint>

// Captured value facade for the PC Bloom byte and Date Number DWORD.
class wxSimpleDatingStateMachine
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x234>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryBloomByteAndDateDword };

    struct ComposedDateForAnalysis
    {
        std::uint32_t beforeRaw = 0;
        CapturedPrefixForAnalysis afterSet{};
        std::uint32_t afterRaw = 0;
    };

    struct ComposedBloomForAnalysis
    {
        std::uint8_t beforeRaw = 0;
        CapturedPrefixForAnalysis afterSet{};
        std::uint8_t afterRaw = 0;
    };

    static bool ReadRawDateForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool WriteRawDateForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t argument,
        CapturedPrefixForAnalysis& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool ComposeRawDateForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t argument,
        ComposedDateForAnalysis& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;

    static bool ReadRawBloomForAnalysis(const CapturedPrefixForAnalysis& before, std::uint8_t& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool WriteBloomLowByteForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t argument,
        CapturedPrefixForAnalysis& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool ComposeBloomLowByteForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t argument,
        ComposedBloomForAnalysis& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
};
