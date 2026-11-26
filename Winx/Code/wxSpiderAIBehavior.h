#pragma once

#include <array>
#include <cstdint>

// Captured value facade exposing the raw byte of the PC Is Spider property.
class wxSpiderAIBehavior
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x1B1>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryIsSpiderRawByte1B0 };

    struct ComposedIsSpiderForAnalysis
    {
        std::uint8_t beforeRaw = 0;
        CapturedPrefixForAnalysis afterSet{};
        std::uint8_t afterRaw = 0;
    };

    static bool ReadIsSpiderByteForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint8_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool WriteIsSpiderLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawArgumentWord, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool ComposeIsSpiderLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawArgumentWord, ComposedIsSpiderForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
};
