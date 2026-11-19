#include "wxPendulum.h"
#include "../Analysis/PC/wxPendulumAbi.h"

namespace
{
using Prefix = wxPendulum::CapturedPrefixForAnalysis;
std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
void WriteWord(Prefix& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t byte = 0; byte < 4; ++byte) bytes[offset + byte] = std::uint8_t(value >> (8 * byte));
}
bool Admit(const Prefix& bytes, wxPendulum::ProfileForAnalysis profile)
{
    return profile == wxPendulum::ProfileForAnalysis::PcCapturedPrimaryRawDwords084128
        && ReadWord(bytes, 0) == wxPendulumAbi::CapturedPrimary;
}
bool Offset(wxPendulum::PropertyForAnalysis property, std::size_t& offset)
{
    switch (property)
    {
    case wxPendulum::PropertyForAnalysis::PeriodRaw128: offset = wxPendulumAbi::PeriodRawOffset; return true;
    case wxPendulum::PropertyForAnalysis::DamageRaw084: offset = wxPendulumAbi::DamageRawOffset; return true;
    default: return false;
    }
}
}

bool wxPendulum::ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t& output, ProfileForAnalysis profile)
{
    std::size_t offset = 0;
    if (!Admit(before, profile) || !Offset(property, offset)) return false;
    output = ReadWord(before, offset);
    return true;
}
bool wxPendulum::WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    std::size_t offset = 0;
    if (!Admit(before, profile) || !Offset(property, offset)) return false;
    auto staged = before;
    WriteWord(staged, offset, rawBits);
    output = staged;
    return true;
}
bool wxPendulum::ComposeRawPropertiesForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawPeriod, std::uint32_t rawDamage, ComposedPropertiesForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedPropertiesForAnalysis staged{};
    auto state = before;
    const auto period = PropertyForAnalysis::PeriodRaw128;
    const auto damage = PropertyForAnalysis::DamageRaw084;
    ReadRawPropertyForAnalysis(state, period, staged.rawGetterResults[0], profile);
    ReadRawPropertyForAnalysis(state, damage, staged.rawGetterResults[1], profile);
    WriteRawPropertyForAnalysis(state, period, rawPeriod, state, profile);
    staged.afterPeriodSet = state;
    ReadRawPropertyForAnalysis(state, period, staged.rawGetterResults[2], profile);
    ReadRawPropertyForAnalysis(state, damage, staged.rawGetterResults[3], profile);
    WriteRawPropertyForAnalysis(state, damage, rawDamage, state, profile);
    staged.afterDamageSet = state;
    ReadRawPropertyForAnalysis(state, period, staged.rawGetterResults[4], profile);
    ReadRawPropertyForAnalysis(state, damage, staged.rawGetterResults[5], profile);
    output = staged;
    return true;
}

#include "Analysis/PC/spFloat80TowardZero.h"
#include <cstring>

namespace
{
using ScalarProperty = wxPendulum::ScalarPropertyForAnalysis;
using ScalarProfile = wxPendulum::ScalarProfileForAnalysis;
using ScalarResult = wxPendulum::ScalarResultForAnalysis;
using ScalarPrefix = wxPendulum::CapturedScalarPrefixForAnalysis;
constexpr std::array<std::size_t, 2> ScalarOffsets{0x124, 0x150};
static_assert(sizeof(wxPendulum::CapturedPrefixForAnalysis) == 300);
static_assert(sizeof(ScalarPrefix) == wxPendulumAbi::CapturedScalarMinimumPrefixBytes);
static_assert(ScalarOffsets.back() + 4 == sizeof(ScalarPrefix));

std::uint32_t ReadScalarWord(const ScalarPrefix& bytes, const std::size_t offset) noexcept
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
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
    if (ReadScalarWord(before, 0) != wxPendulumAbi::CapturedPrimary) return ScalarResult::CapturedPrimaryNotAdmitted;
    const auto index = static_cast<unsigned>(property);
    if (index >= ScalarOffsets.size()) return ScalarResult::UnknownProperty;
    for (const auto selected : ScalarOffsets)
        if (!ScalarWordAdmitted(ReadScalarWord(before, selected))) return ScalarResult::CapturedScalarNotAdmitted;
    offset = ScalarOffsets[index];
    return ScalarResult::Completed;
}
}

wxPendulum::ScalarResultForAnalysis wxPendulum::ReadScalarForAnalysis(
    const CapturedScalarPrefixForAnalysis& before, const ScalarPropertyForAnalysis property,
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

wxPendulum::ScalarResultForAnalysis wxPendulum::WriteScalarBitsForAnalysis(
    const CapturedScalarPrefixForAnalysis& before, const ScalarPropertyForAnalysis property,
    const std::uint32_t rawArgument, CapturedScalarPrefixForAnalysis& output, const ScalarProfileForAnalysis profile) noexcept
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
