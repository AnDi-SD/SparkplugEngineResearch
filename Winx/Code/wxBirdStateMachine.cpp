#include "wxBirdStateMachine.h"
#include "../Analysis/PC/wxBirdStateMachineAbi.h"
#include "Analysis/PC/spFloat80TowardZero.h"

#include <cstring>
#include <limits>

namespace
{
    using Behavior = wxBirdStateMachine;
    using Prefix = Behavior::CapturedPrefixForAnalysis;
    using Property = Behavior::PropertyForAnalysis;
    using Profile = Behavior::ProfileForAnalysis;
    using Result = Behavior::ResultForAnalysis;
    namespace Abi = wxBirdStateMachineAbi;

    static_assert(Prefix{}.size() == Abi::CapturedMinimumPrefixBytes);
    static_assert(Abi::RadiusOffsets[0] + sizeof(std::uint32_t) == Abi::RadiusOffsets[1]);
    static_assert(Abi::RadiusOffsets[1] + sizeof(std::uint32_t) == Abi::CapturedMinimumPrefixBytes);
    static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

    std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset) noexcept
    {
        return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
            | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
    }

    bool IsFiniteNormalOrZero(std::uint32_t bits) noexcept
    {
        const auto exponent = (bits >> 23) & 0xFFu;
        return exponent != 0xFFu && (exponent != 0 || (bits & 0x7FFFFFu) == 0);
    }

    Result Admit(const Prefix& before, Property property, Profile profile, std::size_t& offset) noexcept
    {
        if (profile != Profile::PcCapturedPrimaryRadiiDwords25CAnd260)
            return Result::UnqualifiedProfile;
        if (ReadWord(before, 0) != Abi::CapturedPrimary)
            return Result::CapturedPrimaryNotAdmitted;
        const auto index = static_cast<unsigned>(property);
        if (index >= Abi::RadiusOffsets.size())
            return Result::UnknownProperty;
        offset = Abi::RadiusOffsets[index];
        return Result::Completed;
    }
}

wxBirdStateMachine::ResultForAnalysis wxBirdStateMachine::ReadRadiusForAnalysis(
    const CapturedPrefixForAnalysis& before, PropertyForAnalysis property, RadiusReturnForAnalysis& output,
    ProfileForAnalysis profile) noexcept
{
    std::size_t offset{};
    const auto admitted = Admit(before, property, profile, offset);
    if (admitted != Result::Completed)
        return admitted;
    const auto bits = ReadWord(before, offset);
    if (!IsFiniteNormalOrZero(bits))
        return Result::CapturedRadiusNotAdmitted;

    float value;
    std::memcpy(&value, &bits, sizeof(value));
    const auto number = sparkplug::evidence::pc::float80_rtz::FromFloat(value);
    const auto signAndExponent = std::uint16_t((number.negative ? 0x8000 : 0)
        | (number.mantissa ? unsigned(number.exponent + 63 + 16383) : 0));
    RadiusReturnForAnalysis staged{};
    for (unsigned byte = 0; byte < 8; ++byte)
        staged[byte] = std::uint8_t(number.mantissa >> (8 * byte));
    staged[8] = std::uint8_t(signAndExponent);
    staged[9] = std::uint8_t(signAndExponent >> 8);
    output = staged;
    return Result::Completed;
}

wxBirdStateMachine::ResultForAnalysis wxBirdStateMachine::WriteRadiusBitsForAnalysis(
    const CapturedPrefixForAnalysis& before, PropertyForAnalysis property, std::uint32_t argument,
    CapturedPrefixForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    std::size_t offset{};
    const auto admitted = Admit(before, property, profile, offset);
    if (admitted != Result::Completed)
        return admitted;

    auto staged = before;
    for (unsigned byte = 0; byte < 4; ++byte)
        staged[offset + byte] = std::uint8_t(argument >> (8 * byte));
    output = staged;
    return Result::Completed;
}

wxBirdStateMachine::ResultForAnalysis wxBirdStateMachine::ComposeRadiusForAnalysis(
    const CapturedPrefixForAnalysis& before, PropertyForAnalysis property, std::uint32_t argument,
    ComposedRadiusForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    ComposedRadiusForAnalysis staged;
    const auto first = ReadRadiusForAnalysis(before, property, staged.before, profile);
    if (first != Result::Completed)
        return first;
    if (!IsFiniteNormalOrZero(argument))
        return Result::ArgumentNotAdmitted;
    const auto stored = WriteRadiusBitsForAnalysis(before, property, argument, staged.afterSet, profile);
    if (stored != Result::Completed)
        return stored;
    const auto last = ReadRadiusForAnalysis(staged.afterSet, property, staged.after, profile);
    if (last != Result::Completed)
        return last;
    output = staged;
    return Result::Completed;
}
