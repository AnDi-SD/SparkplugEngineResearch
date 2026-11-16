#include "wxDamageProxy.h"
#include "../Analysis/PC/wxDamageProxyAbi.h"

namespace
{
using Prefix = wxDamageProxy::CapturedPrefixForAnalysis;
std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
void WriteWord(Prefix& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t byte = 0; byte < 4; ++byte) bytes[offset + byte] = std::uint8_t(value >> (8 * byte));
}
bool Admit(const Prefix& bytes, wxDamageProxy::ProfileForAnalysis profile)
{
    return profile == wxDamageProxy::ProfileForAnalysis::PcCapturedPrimaryRawDwords12412C
        && ReadWord(bytes, 0) == wxDamageProxyAbi::CapturedPrimary;
}
bool Offset(wxDamageProxy::PropertyForAnalysis property, std::size_t& offset)
{
    switch (property)
    {
    case wxDamageProxy::PropertyForAnalysis::DamagePeriodRaw12C: offset = wxDamageProxyAbi::DamagePeriodRawOffset; return true;
    case wxDamageProxy::PropertyForAnalysis::DamagePointsRaw124: offset = wxDamageProxyAbi::DamagePointsRawOffset; return true;
    default: return false;
    }
}
}

bool wxDamageProxy::ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t& output, ProfileForAnalysis profile)
{
    std::size_t offset = 0;
    if (!Admit(before, profile) || !Offset(property, offset)) return false;
    output = ReadWord(before, offset);
    return true;
}
bool wxDamageProxy::WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    std::size_t offset = 0;
    if (!Admit(before, profile) || !Offset(property, offset)) return false;
    auto staged = before;
    WriteWord(staged, offset, rawBits);
    output = staged;
    return true;
}
bool wxDamageProxy::ComposeRawPropertiesForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawPeriod, std::uint32_t rawPoints, ComposedPropertiesForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedPropertiesForAnalysis staged{};
    auto state = before;
    const auto period = PropertyForAnalysis::DamagePeriodRaw12C;
    const auto points = PropertyForAnalysis::DamagePointsRaw124;
    ReadRawPropertyForAnalysis(state, period, staged.rawGetterResults[0], profile);
    ReadRawPropertyForAnalysis(state, points, staged.rawGetterResults[1], profile);
    WriteRawPropertyForAnalysis(state, period, rawPeriod, state, profile);
    staged.afterPeriodSet = state;
    ReadRawPropertyForAnalysis(state, period, staged.rawGetterResults[2], profile);
    ReadRawPropertyForAnalysis(state, points, staged.rawGetterResults[3], profile);
    WriteRawPropertyForAnalysis(state, points, rawPoints, state, profile);
    staged.afterPointsSet = state;
    ReadRawPropertyForAnalysis(state, period, staged.rawGetterResults[4], profile);
    ReadRawPropertyForAnalysis(state, points, staged.rawGetterResults[5], profile);
    output = staged;
    return true;
}
