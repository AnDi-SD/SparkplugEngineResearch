#include "wxKickableIce.h"
#include "../Analysis/PC/wxKickableIceAbi.h"
#include "Analysis/PC/spFloat80TowardZero.h"

namespace
{
    using Target = wxKickableIce;
    using Prefix = Target::CapturedPrefixForAnalysis;
    using Property = Target::PropertyForAnalysis;
    using Profile = Target::ProfileForAnalysis;
    namespace Abi = wxKickableIceAbi;

    static_assert(Prefix{}.size() == Abi::CapturedPrefixBytes);
    static_assert(Abi::RawOffsets[0] + sizeof(std::uint32_t) <= Abi::CapturedPrefixBytes);
    static_assert(Abi::RawOffsets[1] + sizeof(std::uint32_t) <= Abi::CapturedPrefixBytes);
    static_assert(Abi::RawOffsets[2] + sizeof(std::uint32_t) <= Abi::CapturedPrefixBytes);

    std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset) noexcept
    {
        return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
            | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
    }

    bool Admit(const Prefix& before, Property property, Profile profile, std::size_t& offset) noexcept
    {
        const auto index = static_cast<unsigned>(property);
        if (profile != Profile::PcCapturedPrimaryIceRawDwords
            || index >= Abi::RawOffsets.size() || ReadWord(before, 0) != Abi::CapturedPrimary)
            return false;
        offset = Abi::RawOffsets[index];
        return true;
    }
}

bool wxKickableIce::ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t& output, ProfileForAnalysis profile) noexcept
{
    std::size_t offset{};
    if (!Admit(before, property, profile, offset))
        return false;
    output = ReadWord(before, offset);
    return true;
}

bool wxKickableIce::WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t rawArgument, CapturedPrefixForAnalysis& output,
    ProfileForAnalysis profile) noexcept
{
    std::size_t offset{};
    if (!Admit(before, property, profile, offset))
        return false;
    auto staged = before;
    for (unsigned byte = 0; byte < 4; ++byte)
        staged[offset + byte] = std::uint8_t(rawArgument >> (8 * byte));
    output = staged;
    return true;
}

bool wxKickableIce::ComposeRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t rawArgument, ComposedRawPropertyForAnalysis& output,
    ProfileForAnalysis profile) noexcept
{
    ComposedRawPropertyForAnalysis staged;
    if (!ReadRawPropertyForAnalysis(before, property, staged.beforeRaw, profile)
        || !WriteRawPropertyForAnalysis(before, property, rawArgument, staged.afterSet, profile)
        || !ReadRawPropertyForAnalysis(staged.afterSet, property, staged.afterRaw, profile))
        return false;
    output = staged;
    return true;
}

namespace
{
    using ImpulseRangeProfile = Target::ImpulseRangeProfileForAnalysis;
    using ImpulseRangeResult = Target::ImpulseRangeResultForAnalysis;

    static_assert(Abi::RawOffsets[2] + sizeof(std::uint32_t) == Abi::ImpulseRangeOffset);
    static_assert(Abi::ImpulseRangeOffset + sizeof(std::uint32_t) <= Abi::CapturedPrefixBytes);

    ImpulseRangeResult AdmitImpulseRange(const Prefix& before, ImpulseRangeProfile profile) noexcept
    {
        if (profile != ImpulseRangeProfile::PcCapturedPrimaryImpulseRangeF32)
            return ImpulseRangeResult::UnqualifiedProfile;
        if (ReadWord(before, 0) != Abi::CapturedPrimary)
            return ImpulseRangeResult::CapturedPrimaryNotAdmitted;
        return ImpulseRangeResult::Completed;
    }

    bool IsPositiveFiniteBinary32(std::uint32_t bits) noexcept
    {
        return bits != 0 && bits < 0x7F800000u;
    }

    Target::ImpulseRangeReturnForAnalysis ExpandPositiveF32(std::uint32_t bits) noexcept
    {
        namespace Float80 = sparkplug::evidence::pc::float80_rtz;

        // Expand the raw significand without a host floating-point conversion.
        const auto exponent = (bits >> 23) & 0xFFu;
        const auto significand = (bits & 0x7FFFFFu) | (exponent != 0 ? 0x800000u : 0u);
        const auto number = Float80::Normalize(Float80::Wide::Shifted(significand, 0),
            int(exponent != 0 ? exponent : 1u) - 150, false);
        const auto biasedExponent = std::uint16_t(number.exponent + 63 + 16383);

        Target::ImpulseRangeReturnForAnalysis output{};
        for (unsigned byte = 0; byte < 8; ++byte)
            output[byte] = std::uint8_t(number.mantissa >> (8 * byte));
        output[8] = std::uint8_t(biasedExponent);
        output[9] = std::uint8_t(biasedExponent >> 8);
        return output;
    }
}

wxKickableIce::ImpulseRangeResultForAnalysis wxKickableIce::ReadImpulseRangeForAnalysis(
    const CapturedPrefixForAnalysis& before, ImpulseRangeReturnForAnalysis& output,
    ImpulseRangeProfileForAnalysis profile) noexcept
{
    const auto admitted = AdmitImpulseRange(before, profile);
    if (admitted != ImpulseRangeResult::Completed)
        return admitted;

    const auto bits = ReadWord(before, Abi::ImpulseRangeOffset);
    if (!IsPositiveFiniteBinary32(bits))
        return ImpulseRangeResult::CapturedRangeNotAdmitted;

    output = ExpandPositiveF32(bits);
    return ImpulseRangeResult::Completed;
}

wxKickableIce::ImpulseRangeResultForAnalysis wxKickableIce::WriteImpulseRangeBitsForAnalysis(
    const CapturedPrefixForAnalysis& before, std::uint32_t argument, CapturedPrefixForAnalysis& output,
    ImpulseRangeProfileForAnalysis profile) noexcept
{
    const auto admitted = AdmitImpulseRange(before, profile);
    if (admitted != ImpulseRangeResult::Completed)
        return admitted;
    if (!IsPositiveFiniteBinary32(argument))
        return ImpulseRangeResult::ArgumentNotAdmitted;

    auto staged = before;
    for (unsigned byte = 0; byte < 4; ++byte)
        staged[Abi::ImpulseRangeOffset + byte] = std::uint8_t(argument >> (8 * byte));
    output = staged;
    return ImpulseRangeResult::Completed;
}

wxKickableIce::ImpulseRangeResultForAnalysis wxKickableIce::ComposeImpulseRangeForAnalysis(
    const CapturedPrefixForAnalysis& before, std::uint32_t argument, ComposedImpulseRangeForAnalysis& output,
    ImpulseRangeProfileForAnalysis profile) noexcept
{
    ComposedImpulseRangeForAnalysis staged;
    const auto first = ReadImpulseRangeForAnalysis(before, staged.before, profile);
    if (first != ImpulseRangeResult::Completed)
        return first;
    const auto stored = WriteImpulseRangeBitsForAnalysis(before, argument, staged.afterSet, profile);
    if (stored != ImpulseRangeResult::Completed)
        return stored;
    const auto last = ReadImpulseRangeForAnalysis(staged.afterSet, staged.after, profile);
    if (last != ImpulseRangeResult::Completed)
        return last;
    output = staged;
    return ImpulseRangeResult::Completed;
}
