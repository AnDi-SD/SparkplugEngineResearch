#include "wxUpsieDaisyGuardAIBehavior.h"
#include "../Analysis/PC/wxUpsieDaisyGuardAIBehaviorAbi.h"
#include "Analysis/PC/spFloat80TowardZero.h"

#include <cstring>
#include <limits>

namespace
{
using B = wxUpsieDaisyGuardAIBehavior;
using Prefix = B::CapturedPrefixForAnalysis;
using ScalarPrefix = B::ScalarPrefixForAnalysis;
using ScalarProperty = B::ScalarPropertyForAnalysis;
using ScalarProfile = B::ScalarProfileForAnalysis;
using ScalarResult = B::ScalarResultForAnalysis;
using BytePrefix = B::BytePrefixForAnalysis;
using ByteProperty = B::BytePropertyForAnalysis;
using ByteProfile = B::ByteProfileForAnalysis;
namespace Abi = wxUpsieDaisyGuardAIBehaviorAbi;

static_assert(ScalarPrefix{}.size() == Abi::ScalarMinimumPrefixBytes);
static_assert(Abi::ScalarOffsets.back() + sizeof(std::uint32_t) == Abi::ScalarMinimumPrefixBytes);
static_assert(BytePrefix{}.size() == Abi::ByteMinimumPrefixBytes);
static_assert(Abi::ByteOffsets.back() + sizeof(std::uint8_t) == Abi::ByteMinimumPrefixBytes);
static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

template<std::size_t N>
std::uint32_t ReadWord(const std::array<std::uint8_t, N>& bytes, std::size_t offset) noexcept
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
bool Admit(const Prefix& bytes, B::ProfileForAnalysis profile)
{
    return profile == B::ProfileForAnalysis::PcCapturedPrimaryRawDword184
        && ReadWord(bytes, 0) == wxUpsieDaisyGuardAIBehaviorAbi::CapturedPrimary;
}

bool IsFiniteNormalOrZero(std::uint32_t bits) noexcept
{
    const auto exponent = (bits >> 23) & 0xFFu;
    return exponent != 0xFFu && (exponent != 0 || (bits & 0x7FFFFFu) == 0);
}

ScalarResult AdmitScalar(const ScalarPrefix& before, ScalarProperty property, ScalarProfile profile,
    std::size_t& offset) noexcept
{
    if (profile != ScalarProfile::PcCapturedPrimaryFiniteNormalOrZeroF32)
        return ScalarResult::UnqualifiedProfile;
    if (ReadWord(before, 0) != Abi::CapturedPrimary)
        return ScalarResult::CapturedPrimaryNotAdmitted;
    const auto index = static_cast<unsigned>(property);
    if (index >= Abi::ScalarOffsets.size())
        return ScalarResult::UnknownProperty;
    for (const auto field : Abi::ScalarOffsets)
    {
        if (!IsFiniteNormalOrZero(ReadWord(before, field)))
            return ScalarResult::CapturedScalarNotAdmitted;
    }
    offset = Abi::ScalarOffsets[index];
    return ScalarResult::Completed;
}

bool AdmitByte(const BytePrefix& before, ByteProperty property, ByteProfile profile,
    std::size_t& offset) noexcept
{
    const auto index = static_cast<unsigned>(property);
    if (profile != ByteProfile::PcCapturedPrimaryRawBytes1D4And1D5
        || index >= Abi::ByteOffsets.size() || ReadWord(before, 0) != Abi::CapturedPrimary)
        return false;
    offset = Abi::ByteOffsets[index];
    return true;
}
}
bool wxUpsieDaisyGuardAIBehavior::ReadGuardNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    output = ReadWord(before, wxUpsieDaisyGuardAIBehaviorAbi::GuardNumRawOffset);
    return true;
}
bool wxUpsieDaisyGuardAIBehavior::WriteGuardNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    auto staged = before;
    for (std::size_t byte = 0; byte < 4; ++byte)
        staged[wxUpsieDaisyGuardAIBehaviorAbi::GuardNumRawOffset + byte] = std::uint8_t(rawBits >> (byte * 8));
    output = staged;
    return true;
}
bool wxUpsieDaisyGuardAIBehavior::ComposeGuardNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, ComposedGuardNumForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedGuardNumForAnalysis staged{};
    auto state = before;
    if (!ReadGuardNumRawForAnalysis(state, staged.beforeRaw, profile)
        || !WriteGuardNumRawForAnalysis(state, rawBits, state, profile)
        || !ReadGuardNumRawForAnalysis(state, staged.afterRaw, profile)) return false;
    staged.afterSet = state;
    output = staged;
    return true;
}

wxUpsieDaisyGuardAIBehavior::ScalarResultForAnalysis wxUpsieDaisyGuardAIBehavior::ReadScalarForAnalysis(
    const ScalarPrefixForAnalysis& before, ScalarPropertyForAnalysis property, ScalarReturnForAnalysis& output,
    ScalarProfileForAnalysis profile) noexcept
{
    std::size_t offset{};
    const auto admitted = AdmitScalar(before, property, profile, offset);
    if (admitted != ScalarResult::Completed)
        return admitted;

    const auto bits = ReadWord(before, offset);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    const auto number = sparkplug::evidence::pc::float80_rtz::FromFloat(value);
    const auto exponent = std::uint16_t((number.negative ? 0x8000 : 0)
        | (number.mantissa ? unsigned(number.exponent + 63 + 16383) : 0));
    ScalarReturnForAnalysis staged{};
    for (unsigned byte = 0; byte < 8; ++byte)
        staged[byte] = std::uint8_t(number.mantissa >> (8 * byte));
    staged[8] = std::uint8_t(exponent);
    staged[9] = std::uint8_t(exponent >> 8);
    output = staged;
    return ScalarResult::Completed;
}

bool wxUpsieDaisyGuardAIBehavior::ReadBytePropertyForAnalysis(const BytePrefixForAnalysis& before,
    BytePropertyForAnalysis property, std::uint8_t& output, ByteProfileForAnalysis profile) noexcept
{
    std::size_t offset{};
    if (!AdmitByte(before, property, profile, offset))
        return false;
    output = before[offset];
    return true;
}

bool wxUpsieDaisyGuardAIBehavior::WriteLowBytePropertyForAnalysis(const BytePrefixForAnalysis& before,
    BytePropertyForAnalysis property, std::uint32_t rawArgumentWord, BytePrefixForAnalysis& output,
    ByteProfileForAnalysis profile) noexcept
{
    std::size_t offset{};
    if (!AdmitByte(before, property, profile, offset))
        return false;
    auto staged = before;
    staged[offset] = std::uint8_t(rawArgumentWord);
    output = staged;
    return true;
}

wxUpsieDaisyGuardAIBehavior::ScalarResultForAnalysis wxUpsieDaisyGuardAIBehavior::WriteScalarBitsForAnalysis(
    const ScalarPrefixForAnalysis& before, ScalarPropertyForAnalysis property, std::uint32_t rawArgument,
    ScalarPrefixForAnalysis& output, ScalarProfileForAnalysis profile) noexcept
{
    std::size_t offset{};
    const auto admitted = AdmitScalar(before, property, profile, offset);
    if (admitted != ScalarResult::Completed)
        return admitted;
    if (!IsFiniteNormalOrZero(rawArgument))
        return ScalarResult::ArgumentNotAdmitted;

    auto staged = before;
    for (unsigned byte = 0; byte < 4; ++byte)
        staged[offset + byte] = std::uint8_t(rawArgument >> (8 * byte));
    output = staged;
    return ScalarResult::Completed;
}
