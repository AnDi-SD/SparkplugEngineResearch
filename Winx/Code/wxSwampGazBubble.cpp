#include "wxSwampGazBubble.h"
#include "../Analysis/PC/wxSwampGazBubbleAbi.h"
#include "Analysis/PC/spFloat80TowardZero.h"
#include <cstring>

namespace
{
using B = wxSwampGazBubble;
using Prefix = B::CapturedPrefixForAnalysis;
std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
bool Admit(const Prefix& bytes, B::ProfileForAnalysis profile)
{
    return profile == B::ProfileForAnalysis::PcCapturedPrimaryRawDword14C
        && ReadWord(bytes, 0) == wxSwampGazBubbleAbi::CapturedPrimary;
}
}
bool wxSwampGazBubble::ReadDamageInflictedRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    output = ReadWord(before, wxSwampGazBubbleAbi::DamageInflictedRawOffset);
    return true;
}
bool wxSwampGazBubble::WriteDamageInflictedRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    auto staged = before;
    for (std::size_t byte = 0; byte < 4; ++byte)
        staged[wxSwampGazBubbleAbi::DamageInflictedRawOffset + byte] = std::uint8_t(rawBits >> (byte * 8));
    output = staged;
    return true;
}
bool wxSwampGazBubble::ComposeDamageInflictedRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, ComposedDamageForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedDamageForAnalysis staged{};
    auto state = before;
    if (!ReadDamageInflictedRawForAnalysis(state, staged.beforeRaw, profile)
        || !WriteDamageInflictedRawForAnalysis(state, rawBits, state, profile)
        || !ReadDamageInflictedRawForAnalysis(state, staged.afterRaw, profile)) return false;
    staged.afterSet = state;
    output = staged;
    return true;
}

namespace
{
bool AdmitGreenByte(const Prefix& bytes, B::ProfileForAnalysis profile)
{
    return profile == B::ProfileForAnalysis::PcCapturedPrimaryRawByte144
        && ReadWord(bytes, 0) == wxSwampGazBubbleAbi::CapturedPrimary;
}
}
bool wxSwampGazBubble::ReadGreenByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint8_t& output, ProfileForAnalysis profile)
{
    if (!AdmitGreenByte(before, profile)) return false;
    output = before[wxSwampGazBubbleAbi::GreenRawByteOffset];
    return true;
}
bool wxSwampGazBubble::WriteGreenLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgumentWord, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    if (!AdmitGreenByte(before, profile)) return false;
    auto staged = before;
    staged[wxSwampGazBubbleAbi::GreenRawByteOffset] = std::uint8_t(rawArgumentWord & 0xFFu);
    output = staged;
    return true;
}
bool wxSwampGazBubble::ComposeGreenLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgumentWord, ComposedGreenByteForAnalysis& output, ProfileForAnalysis profile)
{
    if (!AdmitGreenByte(before, profile)) return false;
    ComposedGreenByteForAnalysis staged{};
    auto state = before;
    if (!ReadGreenByteForAnalysis(state, staged.beforeRaw, profile)
        || !WriteGreenLowByteForAnalysis(state, rawArgumentWord, state, profile)
        || !ReadGreenByteForAnalysis(state, staged.afterRaw, profile)) return false;
    staged.afterSet = state;
    output = staged;
    return true;
}
namespace
{
using ScalarPrefix = wxSwampGazBubble::ScalarCapturedPrefixForAnalysis;
std::uint32_t ReadScalarWord(const ScalarPrefix& bytes, const std::size_t offset) noexcept
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset+1]) << 8)
        | (std::uint32_t(bytes[offset+2]) << 16) | (std::uint32_t(bytes[offset+3]) << 24);
}
using ScalarProperty = wxSwampGazBubble::ScalarPropertyForAnalysis;
using ScalarProfile = wxSwampGazBubble::ScalarProfileForAnalysis;
using ScalarResult = wxSwampGazBubble::ScalarResultForAnalysis;
constexpr std::array<std::size_t, 5> ScalarOffsets{0x15C, 0x158, 0x154, 0x150, 0x148};
static_assert(sizeof(wxSwampGazBubble::ScalarCapturedPrefixForAnalysis) == 352);

void WriteScalarWord(ScalarPrefix& bytes, const std::size_t offset, const std::uint32_t word) noexcept
{
    for (unsigned index = 0; index != 4; ++index)
        bytes[offset + index] = std::uint8_t(word >> (index * 8));
}
bool ScalarWordAdmitted(const std::uint32_t word) noexcept
{
    const auto exponent = (word >> 23) & 255;
    return exponent != 255 && (exponent != 0 || (word & 0x7FFFFF) == 0);
}
ScalarResult AdmitScalar(const ScalarPrefix& before, const ScalarProperty property,
    const ScalarProfile profile, std::size_t& offset) noexcept
{
    if (profile != ScalarProfile::PcCapturedPrimaryFiniteNormalOrZeroF32) return ScalarResult::UnqualifiedProfile;
    if (ReadScalarWord(before, 0) != wxSwampGazBubbleAbi::CapturedPrimary) return ScalarResult::CapturedPrimaryNotAdmitted;
    const auto index = static_cast<unsigned>(property);
    if (index >= ScalarOffsets.size()) return ScalarResult::UnknownProperty;
    for (const auto selected : ScalarOffsets)
        if (!ScalarWordAdmitted(ReadScalarWord(before, selected))) return ScalarResult::CapturedScalarNotAdmitted;
    offset = ScalarOffsets[index];
    return ScalarResult::Completed;
}
}

wxSwampGazBubble::ScalarResultForAnalysis wxSwampGazBubble::ReadScalarForAnalysis(
    const ScalarCapturedPrefixForAnalysis& before, const ScalarPropertyForAnalysis property,
    ScalarReturnForAnalysis& output, const ScalarProfileForAnalysis profile) noexcept
{
    std::size_t offset = 0;
    const auto admitted = AdmitScalar(before, property, profile, offset);
    if (admitted != ScalarResultForAnalysis::Completed) return admitted;
    const auto bits = ReadScalarWord(before, offset);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    const auto number = sparkplug::evidence::pc::float80_rtz::FromFloat(value);
    const auto exponent = std::uint16_t((number.negative ? 0x8000 : 0)
        | (number.mantissa ? unsigned(number.exponent + 63 + 16383) : 0));
    ScalarReturnForAnalysis staged{};
    for (unsigned index = 0; index != 8; ++index)
        staged[index] = std::uint8_t(number.mantissa >> (index * 8));
    staged[8] = std::uint8_t(exponent); staged[9] = std::uint8_t(exponent >> 8);
    output = staged;
    return ScalarResultForAnalysis::Completed;
}

wxSwampGazBubble::ScalarResultForAnalysis wxSwampGazBubble::WriteScalarBitsForAnalysis(
    const ScalarCapturedPrefixForAnalysis& before, const ScalarPropertyForAnalysis property,
    const std::uint32_t rawArgument, ScalarCapturedPrefixForAnalysis& output, const ScalarProfileForAnalysis profile) noexcept
{
    std::size_t offset = 0;
    const auto admitted = AdmitScalar(before, property, profile, offset);
    if (admitted != ScalarResultForAnalysis::Completed) return admitted;
    if (!ScalarWordAdmitted(rawArgument)) return ScalarResultForAnalysis::ArgumentNotAdmitted;
    auto staged = before;
    WriteScalarWord(staged, offset, rawArgument);
    output = staged;
    return ScalarResultForAnalysis::Completed;
}
