#pragma once
#include <array>
#include <cstdint>

// Own captured-value facade, not native layout or physical inheritance.
class wxLever
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 296>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryActionRawDword124 };
    // Uninterpreted DWORD from the descriptor labelled Action; no setter.
    static bool ReadActionTokenRawForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
};
