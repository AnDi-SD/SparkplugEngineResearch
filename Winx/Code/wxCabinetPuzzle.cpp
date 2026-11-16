#include "wxCabinetPuzzle.h"
#include "../Analysis/PC/wxCabinetPuzzleAbi.h"

namespace
{
using F = wxCabinetPuzzle;
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
    return profile == F::ProfileForAnalysis::PcCapturedPrimaryRawDwords13414815C170
        && ReadWord(bytes, 0) == wxCabinetPuzzleAbi::CapturedPrimary;
}
bool Offset(F::PropertyForAnalysis property, std::size_t& offset)
{
    switch (property)
    {
    case F::PropertyForAnalysis::Digit4Raw170: offset = wxCabinetPuzzleAbi::Digit4RawOffset; return true;
    case F::PropertyForAnalysis::Digit3Raw15C: offset = wxCabinetPuzzleAbi::Digit3RawOffset; return true;
    case F::PropertyForAnalysis::Digit2Raw148: offset = wxCabinetPuzzleAbi::Digit2RawOffset; return true;
    case F::PropertyForAnalysis::Digit1Raw134: offset = wxCabinetPuzzleAbi::Digit1RawOffset; return true;
    default: return false;
    }
}
}

bool wxCabinetPuzzle::ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t& output, ProfileForAnalysis profile)
{
    std::size_t offset = 0;
    if (!Admit(before, profile) || !Offset(property, offset)) return false;
    output = ReadWord(before, offset);
    return true;
}
bool wxCabinetPuzzle::WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    std::size_t offset = 0;
    if (!Admit(before, profile) || !Offset(property, offset)) return false;
    auto staged = before;
    WriteWord(staged, offset, rawBits);
    output = staged;
    return true;
}
bool wxCabinetPuzzle::ComposeRawPropertiesForAnalysis(const CapturedPrefixForAnalysis& before,
    const std::array<std::uint32_t, 4>& rawArguments, ComposedPropertiesForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    constexpr std::array<PropertyForAnalysis, 4> properties = {PropertyForAnalysis::Digit4Raw170,
        PropertyForAnalysis::Digit3Raw15C, PropertyForAnalysis::Digit2Raw148, PropertyForAnalysis::Digit1Raw134};
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
