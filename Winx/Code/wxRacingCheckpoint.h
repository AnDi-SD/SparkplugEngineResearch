#pragma once
#include <array>
#include <cstdint>

// Captured value facade. This does not declare the native object's full layout.
class wxRacingCheckpoint
{
public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x13C>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryRawDwords128138, PcCapturedPrimaryFinishLowByte140Prefix141 };
    enum class PropertyForAnalysis { BonusTimeRaw138, CheckpointIdRaw128 };
    struct ComposedPropertiesForAnalysis
    {
        std::array<std::uint32_t, 6> rawGetterResults{};
        CapturedPrefixForAnalysis afterBonusTimeSet{};
        CapturedPrefixForAnalysis afterCheckpointIdSet{};
    };
    static bool ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
        PropertyForAnalysis property, std::uint32_t rawBits, CapturedPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeRawPropertiesForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t rawBonusTime, std::uint32_t rawCheckpointId, ComposedPropertiesForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    // Distinct value prefix. The accepted RAW316 type and methods stay unchanged.
    using CapturedFinishPrefixForAnalysis = std::array<std::uint8_t, 0x141>;
    struct ComposedFinishByteForAnalysis
    {
        std::uint8_t beforeRaw{};
        CapturedFinishPrefixForAnalysis afterSet{};
        std::uint8_t afterRaw{};
    };
    static bool ReadFinishByteForAnalysis(const CapturedFinishPrefixForAnalysis& before,
        std::uint8_t& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool WriteFinishLowByteForAnalysis(const CapturedFinishPrefixForAnalysis& before,
        std::uint32_t rawArgumentWord, CapturedFinishPrefixForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
    static bool ComposeFinishLowByteForAnalysis(const CapturedFinishPrefixForAnalysis& before,
        std::uint32_t rawArgumentWord, ComposedFinishByteForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified);
};
