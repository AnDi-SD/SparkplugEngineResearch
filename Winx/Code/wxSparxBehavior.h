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

    enum class TypeProfileForAnalysis { Unqualified, PcCapturedPrimarySparxTypeRawDword124 };
    struct ComposedSparxTypeRawForAnalysis
    {
        std::uint32_t beforeRaw{};
        CapturedPrefixForAnalysis afterSet{};
        std::uint32_t afterRaw{};
    };

    // Raw PC words, including every bit of the supplied setter argument.
    static bool ReadSparxTypeRawForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t& output,
        TypeProfileForAnalysis profile = TypeProfileForAnalysis::Unqualified) noexcept;
    static bool WriteSparxTypeRawForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t rawArgumentWord,
        CapturedPrefixForAnalysis& output, TypeProfileForAnalysis profile = TypeProfileForAnalysis::Unqualified) noexcept;
    static bool ComposeSparxTypeRawForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t rawArgumentWord,
        ComposedSparxTypeRawForAnalysis& output, TypeProfileForAnalysis profile = TypeProfileForAnalysis::Unqualified) noexcept;
};
