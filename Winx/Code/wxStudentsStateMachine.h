#pragma once

#include <array>
#include <cstdint>

// Captured value facade for the raw PC Troll Fight property byte.
class wxStudentsStateMachine
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x285>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryTrollFightRawByte284 };

    struct ComposedTrollFightForAnalysis
    {
        std::uint8_t beforeRaw = 0;
        CapturedPrefixForAnalysis afterSet{};
        std::uint8_t afterRaw = 0;
    };

    static bool ReadTrollFightByteForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint8_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool WriteTrollFightLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawArgumentWord, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool ComposeTrollFightLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawArgumentWord, ComposedTrollFightForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
};
