#include "wxMinotaurAIBehavior.h"
#include "../Analysis/PC/wxMinotaurAIBehaviorAbi.h"
#include "Analysis/PC/spFloat80TowardZero.h"

#include <cstring>
#include <limits>

namespace
{
    using Behavior = wxMinotaurAIBehavior;
    using Prefix = Behavior::CapturedPrefixForAnalysis;
    using Profile = Behavior::ProfileForAnalysis;
    namespace Abi = wxMinotaurAIBehaviorAbi;

    static_assert(Prefix{}.size() == Abi::CapturedMinimumPrefixBytes);
    static_assert(Abi::IdOffset + 4 <= Abi::Ct2Offset);
    static_assert(Abi::Ct2Offset + 1 == Abi::CapturedMinimumPrefixBytes);

    std::uint32_t ReadWord(const Prefix& prefix, std::size_t offset) noexcept
    {
        return std::uint32_t(prefix[offset]) | (std::uint32_t(prefix[offset + 1]) << 8)
            | (std::uint32_t(prefix[offset + 2]) << 16) | (std::uint32_t(prefix[offset + 3]) << 24);
    }

    bool Admit(const Prefix& before, Profile profile) noexcept
    {
        return profile == Profile::PcCapturedPrimaryIdDwordAndCt2Byte && ReadWord(before, 0) == Abi::CapturedPrimary;
    }

    using EnergyProfile = Behavior::EnergyProfileForAnalysis;
    using EnergyResult = Behavior::EnergyResultForAnalysis;

    static_assert(Abi::IdOffset + sizeof(std::uint32_t) == Abi::EnergyOffset);
    static_assert(Abi::EnergyOffset + sizeof(std::uint32_t) < Abi::Ct2Offset);
    static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

    EnergyResult AdmitEnergy(const Prefix& before, EnergyProfile profile) noexcept
    {
        if (profile != EnergyProfile::PcCapturedPrimaryEnergyDword1B4)
            return EnergyResult::UnqualifiedProfile;
        if (ReadWord(before, 0) != Abi::CapturedPrimary)
            return EnergyResult::CapturedPrimaryNotAdmitted;
        return EnergyResult::Completed;
    }

    bool IsFiniteNormalOrZero(std::uint32_t bits) noexcept
    {
        const auto exponent = (bits >> 23) & 0xFFu;
        return exponent != 0xFFu && (exponent != 0 || (bits & 0x7FFFFFu) == 0);
    }
}

bool wxMinotaurAIBehavior::ReadRawIdForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    output = ReadWord(before, Abi::IdOffset);
    return true;
}

bool wxMinotaurAIBehavior::WriteRawIdForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t argument,
    CapturedPrefixForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    auto staged = before;
    for (std::size_t byte = 0; byte < 4; ++byte)
        staged[Abi::IdOffset + byte] = std::uint8_t(argument >> (8 * byte));
    output = staged;
    return true;
}

bool wxMinotaurAIBehavior::ComposeRawIdForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t argument,
    ComposedIdForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    ComposedIdForAnalysis staged;
    if (!ReadRawIdForAnalysis(before, staged.beforeRaw, profile)
        || !WriteRawIdForAnalysis(before, argument, staged.afterSet, profile)
        || !ReadRawIdForAnalysis(staged.afterSet, staged.afterRaw, profile))
        return false;
    output = staged;
    return true;
}

bool wxMinotaurAIBehavior::ReadRawCt2ByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint8_t& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    output = before[Abi::Ct2Offset];
    return true;
}

bool wxMinotaurAIBehavior::WriteCt2LowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t argument, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    auto staged = before;
    staged[Abi::Ct2Offset] = std::uint8_t(argument);
    output = staged;
    return true;
}

bool wxMinotaurAIBehavior::ComposeCt2LowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t argument, ComposedCt2ForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    ComposedCt2ForAnalysis staged;
    if (!ReadRawCt2ByteForAnalysis(before, staged.beforeRaw, profile)
        || !WriteCt2LowByteForAnalysis(before, argument, staged.afterSet, profile)
        || !ReadRawCt2ByteForAnalysis(staged.afterSet, staged.afterRaw, profile))
        return false;
    output = staged;
    return true;
}

wxMinotaurAIBehavior::EnergyResultForAnalysis wxMinotaurAIBehavior::ReadEnergyForAnalysis(
    const CapturedPrefixForAnalysis& before, EnergyReturnForAnalysis& output, EnergyProfileForAnalysis profile) noexcept
{
    const auto admitted = AdmitEnergy(before, profile);
    if (admitted != EnergyResult::Completed)
        return admitted;

    const auto bits = ReadWord(before, Abi::EnergyOffset);
    if (!IsFiniteNormalOrZero(bits))
        return EnergyResult::CapturedEnergyNotAdmitted;

    float value;
    std::memcpy(&value, &bits, sizeof(value));
    const auto number = sparkplug::evidence::pc::float80_rtz::FromFloat(value);
    const auto signAndExponent = std::uint16_t((number.negative ? 0x8000 : 0)
        | (number.mantissa ? unsigned(number.exponent + 63 + 16383) : 0));
    EnergyReturnForAnalysis staged{};
    for (unsigned byte = 0; byte < 8; ++byte)
        staged[byte] = std::uint8_t(number.mantissa >> (8 * byte));
    staged[8] = std::uint8_t(signAndExponent);
    staged[9] = std::uint8_t(signAndExponent >> 8);
    output = staged;
    return EnergyResult::Completed;
}

wxMinotaurAIBehavior::EnergyResultForAnalysis wxMinotaurAIBehavior::WriteEnergyBitsForAnalysis(
    const CapturedPrefixForAnalysis& before, std::uint32_t argument, CapturedPrefixForAnalysis& output,
    EnergyProfileForAnalysis profile) noexcept
{
    const auto admitted = AdmitEnergy(before, profile);
    if (admitted != EnergyResult::Completed)
        return admitted;

    auto staged = before;
    for (unsigned byte = 0; byte < 4; ++byte)
        staged[Abi::EnergyOffset + byte] = std::uint8_t(argument >> (8 * byte));
    output = staged;
    return EnergyResult::Completed;
}

wxMinotaurAIBehavior::EnergyResultForAnalysis wxMinotaurAIBehavior::ComposeEnergyForAnalysis(
    const CapturedPrefixForAnalysis& before, std::uint32_t argument, ComposedEnergyForAnalysis& output,
    EnergyProfileForAnalysis profile) noexcept
{
    ComposedEnergyForAnalysis staged;
    const auto first = ReadEnergyForAnalysis(before, staged.before, profile);
    if (first != EnergyResult::Completed)
        return first;
    if (!IsFiniteNormalOrZero(argument))
        return EnergyResult::ArgumentNotAdmitted;
    const auto stored = WriteEnergyBitsForAnalysis(before, argument, staged.afterSet, profile);
    if (stored != EnergyResult::Completed)
        return stored;
    const auto last = ReadEnergyForAnalysis(staged.afterSet, staged.after, profile);
    if (last != EnergyResult::Completed)
        return last;
    output = staged;
    return EnergyResult::Completed;
}
