#include "wxWandringNPCAIBehavior.h"
#include "../Analysis/PC/wxWandringNPCAIBehaviorAbi.h"
#include "Analysis/PC/spFloat80TowardZero.h"

#include <cstring>
#include <limits>

namespace
{
    using Behavior = wxWandringNPCAIBehavior;
    using Prefix = Behavior::CapturedPrefixForAnalysis;
    using Profile = Behavior::ProfileForAnalysis;
    using Result = Behavior::ResultForAnalysis;
    namespace Abi = wxWandringNPCAIBehaviorAbi;

    static_assert(Prefix{}.size() == Abi::CapturedMinimumPrefixBytes);
    static_assert(Abi::AmplitudeOffset + sizeof(std::uint32_t) == Abi::CapturedMinimumPrefixBytes);
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

    Result Admit(const Prefix& before, Profile profile) noexcept
    {
        if (profile != Profile::PcCapturedPrimaryFiniteNormalOrZeroF32)
            return Result::UnqualifiedProfile;
        if (ReadWord(before, 0) != Abi::CapturedPrimary)
            return Result::CapturedPrimaryNotAdmitted;
        if (!IsFiniteNormalOrZero(ReadWord(before, Abi::AmplitudeOffset)))
            return Result::CapturedAmplitudeNotAdmitted;
        return Result::Completed;
    }
}

wxWandringNPCAIBehavior::ResultForAnalysis wxWandringNPCAIBehavior::ReadAmplitudeForAnalysis(
    const CapturedPrefixForAnalysis& before, AmplitudeReturnForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    const auto admitted = Admit(before, profile);
    if (admitted != Result::Completed)
        return admitted;

    const auto bits = ReadWord(before, Abi::AmplitudeOffset);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    const auto number = sparkplug::evidence::pc::float80_rtz::FromFloat(value);
    const auto signAndExponent = std::uint16_t((number.negative ? 0x8000 : 0)
        | (number.mantissa ? unsigned(number.exponent + 63 + 16383) : 0));
    AmplitudeReturnForAnalysis staged{};
    for (unsigned byte = 0; byte < 8; ++byte)
        staged[byte] = std::uint8_t(number.mantissa >> (8 * byte));
    staged[8] = std::uint8_t(signAndExponent);
    staged[9] = std::uint8_t(signAndExponent >> 8);
    output = staged;
    return Result::Completed;
}

wxWandringNPCAIBehavior::ResultForAnalysis wxWandringNPCAIBehavior::WriteAmplitudeBitsForAnalysis(
    const CapturedPrefixForAnalysis& before, std::uint32_t argument, CapturedPrefixForAnalysis& output,
    ProfileForAnalysis profile) noexcept
{
    const auto admitted = Admit(before, profile);
    if (admitted != Result::Completed)
        return admitted;
    if (!IsFiniteNormalOrZero(argument))
        return Result::ArgumentNotAdmitted;

    auto staged = before;
    for (unsigned byte = 0; byte < 4; ++byte)
        staged[Abi::AmplitudeOffset + byte] = std::uint8_t(argument >> (8 * byte));
    output = staged;
    return Result::Completed;
}

wxWandringNPCAIBehavior::ResultForAnalysis wxWandringNPCAIBehavior::ComposeAmplitudeForAnalysis(
    const CapturedPrefixForAnalysis& before, std::uint32_t argument, ComposedAmplitudeForAnalysis& output,
    ProfileForAnalysis profile) noexcept
{
    ComposedAmplitudeForAnalysis staged;
    const auto first = ReadAmplitudeForAnalysis(before, staged.before, profile);
    if (first != Result::Completed)
        return first;
    const auto stored = WriteAmplitudeBitsForAnalysis(before, argument, staged.afterSet, profile);
    if (stored != Result::Completed)
        return stored;
    const auto last = ReadAmplitudeForAnalysis(staged.afterSet, staged.after, profile);
    if (last != Result::Completed)
        return last;
    output = staged;
    return Result::Completed;
}
