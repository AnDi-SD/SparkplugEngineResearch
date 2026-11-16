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
