#pragma once

#include <array>
#include <cstdint>

// Captured value facade for the PC Stunned Time descriptor.
class wxGolemAIBehavior
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x1DC>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryStunnedTimeRawDword1D8 };

    struct ComposedStunnedTimeForAnalysis
    {
        std::uint32_t beforeRaw = 0;
        CapturedPrefixForAnalysis afterSet{};
        std::uint32_t afterRaw = 0;
    };

    static bool ReadStunnedTimeRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool WriteStunnedTimeRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawArgument, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool ComposeStunnedTimeRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawArgument, ComposedStunnedTimeForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
};
