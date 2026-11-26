#pragma once

#include <array>
#include <cstdint>

// Captured value facade for the PC ID, Energy and Ct2 descriptor leaves.
class wxMinotaurAIBehavior
{
  public:
    using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x1BA>;
    enum class ProfileForAnalysis { Unqualified, PcCapturedPrimaryIdDwordAndCt2Byte };

    struct ComposedIdForAnalysis
    {
        std::uint32_t beforeRaw = 0;
        CapturedPrefixForAnalysis afterSet{};
        std::uint32_t afterRaw = 0;
    };

    struct ComposedCt2ForAnalysis
    {
        std::uint8_t beforeRaw = 0;
        CapturedPrefixForAnalysis afterSet{};
        std::uint8_t afterRaw = 0;
    };

    static bool ReadRawIdForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool WriteRawIdForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t argument,
        CapturedPrefixForAnalysis& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool ComposeRawIdForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t argument,
        ComposedIdForAnalysis& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;

    static bool ReadRawCt2ByteForAnalysis(const CapturedPrefixForAnalysis& before, std::uint8_t& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool WriteCt2LowByteForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t argument,
        CapturedPrefixForAnalysis& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    static bool ComposeCt2LowByteForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t argument,
        ComposedCt2ForAnalysis& output, ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;

    using EnergyReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class EnergyProfileForAnalysis { Unqualified, PcCapturedPrimaryEnergyDword1B4 };
    enum class EnergyResultForAnalysis
    {
        Completed,
        UnqualifiedProfile,
        CapturedPrimaryNotAdmitted,
        CapturedEnergyNotAdmitted,
        ArgumentNotAdmitted,
    };

    struct ComposedEnergyForAnalysis
    {
        EnergyReturnForAnalysis before{};
        CapturedPrefixForAnalysis afterSet{};
        EnergyReturnForAnalysis after{};
    };

    static EnergyResultForAnalysis ReadEnergyForAnalysis(const CapturedPrefixForAnalysis& before,
        EnergyReturnForAnalysis& output, EnergyProfileForAnalysis profile = EnergyProfileForAnalysis::Unqualified) noexcept;
    static EnergyResultForAnalysis WriteEnergyBitsForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t argument, CapturedPrefixForAnalysis& output,
        EnergyProfileForAnalysis profile = EnergyProfileForAnalysis::Unqualified) noexcept;
    static EnergyResultForAnalysis ComposeEnergyForAnalysis(const CapturedPrefixForAnalysis& before,
        std::uint32_t argument, ComposedEnergyForAnalysis& output,
        EnergyProfileForAnalysis profile = EnergyProfileForAnalysis::Unqualified) noexcept;
};
