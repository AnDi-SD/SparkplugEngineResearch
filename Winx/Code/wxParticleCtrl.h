#pragma once
#include <array>
#include <cstdint>

// Own captured-value facade, not native layout or physical inheritance.
class wxParticleCtrl
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 296>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryAnimationFileRawDword124 };
    // Uninterpreted DWORD from the descriptor labelled Animation file; no setter.
    static bool ReadAnimationFileRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
};
