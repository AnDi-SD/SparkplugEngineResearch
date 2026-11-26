#include "wxSpiderUpsieDaisyAIBehavior.h"
#include "../Analysis/PC/wxSpiderUpsieDaisyAIBehaviorAbi.h"
#include "Analysis/PC/spFloat80TowardZero.h"

#include <cstring>
#include <limits>

namespace
{
using B = wxSpiderUpsieDaisyAIBehavior;
using Prefix = B::CapturedPrefixForAnalysis;
using SpeedPrefix = B::SpeedPrefixForAnalysis;
using SpeedProperty = B::SpeedPropertyForAnalysis;
using SpeedProfile = B::SpeedProfileForAnalysis;
using SpeedResult = B::SpeedResultForAnalysis;
namespace Abi = wxSpiderUpsieDaisyAIBehaviorAbi;

static_assert(SpeedPrefix{}.size() == Abi::SpeedMinimumPrefixBytes);
static_assert(Abi::SpeedOffsets.back() + sizeof(std::uint32_t) == Abi::SpeedMinimumPrefixBytes);
static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

template<std::size_t N>
std::uint32_t ReadWord(const std::array<std::uint8_t, N>& bytes, std::size_t offset) noexcept
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
bool Admit(const Prefix& bytes, B::ProfileForAnalysis profile)
{
    return profile == B::ProfileForAnalysis::PcCapturedPrimaryRawDword134
        && ReadWord(bytes, 0) == wxSpiderUpsieDaisyAIBehaviorAbi::CapturedPrimary;
}

bool IsFiniteNormalOrZero(std::uint32_t bits) noexcept
{
    const auto exponent = (bits >> 23) & 0xFFu;
    return exponent != 0xFFu && (exponent != 0 || (bits & 0x7FFFFFu) == 0);
}

SpeedResult AdmitSpeed(const SpeedPrefix& before, SpeedProperty property, SpeedProfile profile,
    std::size_t& offset) noexcept
{
    if (profile != SpeedProfile::PcCapturedPrimaryFiniteNormalOrZeroF32)
        return SpeedResult::UnqualifiedProfile;
    if (ReadWord(before, 0) != Abi::CapturedPrimary)
        return SpeedResult::CapturedPrimaryNotAdmitted;
    const auto index = static_cast<unsigned>(property);
    if (index >= Abi::SpeedOffsets.size())
        return SpeedResult::UnknownProperty;
    for (const auto field : Abi::SpeedOffsets)
    {
        if (!IsFiniteNormalOrZero(ReadWord(before, field)))
            return SpeedResult::CapturedSpeedNotAdmitted;
    }
    offset = Abi::SpeedOffsets[index];
    return SpeedResult::Completed;
}
}
bool wxSpiderUpsieDaisyAIBehavior::ReadSpiderNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    output = ReadWord(before, wxSpiderUpsieDaisyAIBehaviorAbi::SpiderNumRawOffset);
    return true;
}
bool wxSpiderUpsieDaisyAIBehavior::WriteSpiderNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    auto staged = before;
    for (std::size_t byte = 0; byte < 4; ++byte)
        staged[wxSpiderUpsieDaisyAIBehaviorAbi::SpiderNumRawOffset + byte] = std::uint8_t(rawBits >> (byte * 8));
    output = staged;
    return true;
}
bool wxSpiderUpsieDaisyAIBehavior::ComposeSpiderNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, ComposedSpiderNumForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedSpiderNumForAnalysis staged{};
    auto state = before;
    if (!ReadSpiderNumRawForAnalysis(state, staged.beforeRaw, profile)
        || !WriteSpiderNumRawForAnalysis(state, rawBits, state, profile)
        || !ReadSpiderNumRawForAnalysis(state, staged.afterRaw, profile)) return false;
    staged.afterSet = state;
    output = staged;
    return true;
}

wxSpiderUpsieDaisyAIBehavior::SpeedResultForAnalysis wxSpiderUpsieDaisyAIBehavior::ReadSpeedForAnalysis(
    const SpeedPrefixForAnalysis& before, SpeedPropertyForAnalysis property, SpeedReturnForAnalysis& output,
    SpeedProfileForAnalysis profile) noexcept
{
    std::size_t offset{};
    const auto admitted = AdmitSpeed(before, property, profile, offset);
    if (admitted != SpeedResult::Completed)
        return admitted;

    const auto bits = ReadWord(before, offset);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    const auto number = sparkplug::evidence::pc::float80_rtz::FromFloat(value);
    const auto exponent = std::uint16_t((number.negative ? 0x8000 : 0)
        | (number.mantissa ? unsigned(number.exponent + 63 + 16383) : 0));
    SpeedReturnForAnalysis staged{};
    for (unsigned byte = 0; byte < 8; ++byte)
        staged[byte] = std::uint8_t(number.mantissa >> (8 * byte));
    staged[8] = std::uint8_t(exponent);
    staged[9] = std::uint8_t(exponent >> 8);
    output = staged;
    return SpeedResult::Completed;
}

wxSpiderUpsieDaisyAIBehavior::SpeedResultForAnalysis wxSpiderUpsieDaisyAIBehavior::WriteSpeedBitsForAnalysis(
    const SpeedPrefixForAnalysis& before, SpeedPropertyForAnalysis property, std::uint32_t rawArgument,
    SpeedPrefixForAnalysis& output, SpeedProfileForAnalysis profile) noexcept
{
    std::size_t offset{};
    const auto admitted = AdmitSpeed(before, property, profile, offset);
    if (admitted != SpeedResult::Completed)
        return admitted;
    if (!IsFiniteNormalOrZero(rawArgument))
        return SpeedResult::ArgumentNotAdmitted;

    auto staged = before;
    for (unsigned byte = 0; byte < 4; ++byte)
        staged[offset + byte] = std::uint8_t(rawArgument >> (8 * byte));
    output = staged;
    return SpeedResult::Completed;
}
