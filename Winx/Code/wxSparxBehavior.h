#pragma once
#include <array>
#include <cstdint>

// Own bounded value facade; this is not the native C++ layout or inheritance.
class wxSparxBehavior
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 297>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryTestAnimationRawByte128 };
    struct ComposedTestAnimationByteForAnalysis
    {
        std::uint8_t beforeRaw{};
        CapturedPrefixForAnalysis afterSet{};
        std::uint8_t afterRaw{};
    };
    // Byte values only; no native bool, full EAX, or native return ABI is exposed.
    static bool ReadTestAnimationByteForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint8_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteTestAnimationLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawArgumentWord, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeTestAnimationLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawArgumentWord, ComposedTestAnimationByteForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
};
