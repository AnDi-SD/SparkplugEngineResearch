#pragma once

#include <array>
#include <cstdint>

// Analysis facade for a captured receiver prefix.
class wxOneLinerMovingCharacter
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 296>;

    enum class ProfileForAnalysis
    {
        Unqualified,
        PcCapturedPrimaryOneLinerIdRawDword124,
    };

    // Read the raw DWORD for the eOneLinerID descriptor.
    static bool ReadOneLinerIdRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);

    // Store the raw DWORD in the caller-owned receiver prefix.
    static bool WriteOneLinerIdRawForAnalysis(CapturedPrefixForAnalysis& prefix,
        std::uint32_t value, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
};
