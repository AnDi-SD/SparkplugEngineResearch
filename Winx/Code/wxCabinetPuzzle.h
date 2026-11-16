#pragma once
#include <array>
#include <cstdint>

// Captured values only. This facade does not declare the native object layout.
class wxCabinetPuzzle
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x174>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDwords13414815C170 };
    enum class PropertyForAnalysis { Digit4Raw170, Digit3Raw15C, Digit2Raw148, Digit1Raw134 };
    struct ComposedPropertiesForAnalysis
    {
        // Publisher order; each pair is GET before SET and GET after SET.
        std::array<std::uint32_t, 8> rawGetterResults{};
        std::array<CapturedPrefixForAnalysis, 4> afterSet{};
    };
    static bool ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t rawBits, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeRawPropertiesForAnalysis(const CapturedPrefixForAnalysis& before,
        const std::array<std::uint32_t, 4>& rawArguments, ComposedPropertiesForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
};
