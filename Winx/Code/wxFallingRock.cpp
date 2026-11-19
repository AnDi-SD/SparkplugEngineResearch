#include "wxFallingRock.h"
#include "../Analysis/PC/wxFallingRockAbi.h"
#include "Analysis/PC/spFloat80TowardZero.h"
#include <cstring>

namespace
{
using F = wxFallingRock;
using Prefix = F::CapturedPrefixForAnalysis;
std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
void WriteWord(Prefix& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t byte = 0; byte < 4; ++byte) bytes[offset + byte] = std::uint8_t(value >> (8 * byte));
}
bool Admit(const Prefix& bytes, F::ProfileForAnalysis profile)
{
    return profile == F::ProfileForAnalysis::PcCapturedPrimaryRawDwords170180184188
        && ReadWord(bytes, 0) == wxFallingRockAbi::CapturedPrimary;
}
bool Offset(F::PropertyForAnalysis property, std::size_t& offset)
{
    switch (property)
    {
    case F::PropertyForAnalysis::WarningDeltaRaw188: offset = wxFallingRockAbi::WarningDeltaRawOffset; return true;
    case F::PropertyForAnalysis::FallingDurationRaw184: offset = wxFallingRockAbi::FallingDurationRawOffset; return true;
    case F::PropertyForAnalysis::FallingIntervalRaw180: offset = wxFallingRockAbi::FallingIntervalRawOffset; return true;
    case F::PropertyForAnalysis::InflictedDamageRaw170: offset = wxFallingRockAbi::InflictedDamageRawOffset; return true;
    default: return false;
    }
}
}

bool wxFallingRock::ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t& output, ProfileForAnalysis profile)
{
    std::size_t offset = 0;
    if (!Admit(before, profile) || !Offset(property, offset)) return false;
    output = ReadWord(before, offset);
    return true;
}
bool wxFallingRock::WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    std::size_t offset = 0;
    if (!Admit(before, profile) || !Offset(property, offset)) return false;
    auto staged = before;
    WriteWord(staged, offset, rawBits);
    output = staged;
    return true;
}
bool wxFallingRock::ComposeRawPropertiesForAnalysis(const CapturedPrefixForAnalysis& before,
    const std::array<std::uint32_t, 4>& rawArguments, ComposedPropertiesForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    constexpr std::array<PropertyForAnalysis, 4> properties = {PropertyForAnalysis::WarningDeltaRaw188,
        PropertyForAnalysis::FallingDurationRaw184, PropertyForAnalysis::FallingIntervalRaw180, PropertyForAnalysis::InflictedDamageRaw170};
    ComposedPropertiesForAnalysis staged{};
    auto state = before;
    for (std::size_t index = 0; index < properties.size(); ++index)
    {
        if (!ReadRawPropertyForAnalysis(state, properties[index], staged.rawGetterResults[index * 2], profile)
            || !WriteRawPropertyForAnalysis(state, properties[index], rawArguments[index], state, profile)
            || !ReadRawPropertyForAnalysis(state, properties[index], staged.rawGetterResults[index * 2 + 1], profile)) return false;
        staged.afterSet[index] = state;
    }
    output = staged;
    return true;
}

namespace
{
using ScalarProperty = wxFallingRock::ScalarPropertyForAnalysis;
using ScalarProfile = wxFallingRock::ScalarProfileForAnalysis;
using ScalarResult = wxFallingRock::ScalarResultForAnalysis;
constexpr std::array<std::size_t, 6> ScalarOffsets{0x124, 0x134, 0x178, 0x174, 0x144, 0x148};
static_assert(sizeof(wxFallingRock::CapturedPrefixForAnalysis) == wxFallingRockAbi::CapturedMinimumPrefixBytes);

bool ScalarWordAdmitted(const std::uint32_t word) noexcept
{
    const auto exponent = (word >> 23) & 255;
    return exponent != 255 && (exponent != 0 || (word & 0x7FFFFF) == 0);
}
ScalarResult AdmitScalar(const Prefix& before, const ScalarProperty property,
    const ScalarProfile profile, std::size_t& offset) noexcept
{
    if (profile != ScalarProfile::PcCapturedPrimaryFiniteNormalOrZeroF32) return ScalarResult::UnqualifiedProfile;
    if (ReadWord(before, 0) != wxFallingRockAbi::CapturedPrimary) return ScalarResult::CapturedPrimaryNotAdmitted;
    const auto index = static_cast<unsigned>(property);
    if (index >= ScalarOffsets.size()) return ScalarResult::UnknownProperty;
    for (const auto selected : ScalarOffsets)
        if (!ScalarWordAdmitted(ReadWord(before, selected))) return ScalarResult::CapturedScalarNotAdmitted;
    offset = ScalarOffsets[index];
    return ScalarResult::Completed;
}
}

wxFallingRock::ScalarResultForAnalysis wxFallingRock::ReadScalarForAnalysis(
    const CapturedPrefixForAnalysis& before, const ScalarPropertyForAnalysis property,
    ScalarReturnForAnalysis& output, const ScalarProfileForAnalysis profile) noexcept
{
    std::size_t offset = 0;
    const auto admitted = AdmitScalar(before, property, profile, offset);
    if (admitted != ScalarResultForAnalysis::Completed) return admitted;
    const auto bits = ReadWord(before, offset);
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

wxFallingRock::ScalarResultForAnalysis wxFallingRock::WriteScalarBitsForAnalysis(
    const CapturedPrefixForAnalysis& before, const ScalarPropertyForAnalysis property,
    const std::uint32_t rawArgument, CapturedPrefixForAnalysis& output, const ScalarProfileForAnalysis profile) noexcept
{
    std::size_t offset = 0;
    const auto admitted = AdmitScalar(before, property, profile, offset);
    if (admitted != ScalarResultForAnalysis::Completed) return admitted;
    if (!ScalarWordAdmitted(rawArgument)) return ScalarResultForAnalysis::ArgumentNotAdmitted;
    auto staged = before;
    WriteWord(staged, offset, rawArgument);
    output = staged;
    return ScalarResultForAnalysis::Completed;
}
