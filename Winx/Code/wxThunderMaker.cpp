#include "wxThunderMaker.h"
#include "../Analysis/PC/wxThunderMakerAbi.h"

namespace
{
using Prefix = wxThunderMaker::CapturedPrefixForAnalysis;
std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
void WriteWord(Prefix& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t byte = 0; byte < 4; ++byte) bytes[offset + byte] = std::uint8_t(value >> (8 * byte));
}
bool Admit(const Prefix& bytes, wxThunderMaker::ProfileForAnalysis profile)
{
    return profile == wxThunderMaker::ProfileForAnalysis::PcCapturedPrimaryRawDwords12812C
        && ReadWord(bytes, 0) == wxThunderMakerAbi::CapturedPrimary;
}
bool Offset(wxThunderMaker::PropertyForAnalysis property, std::size_t& offset)
{
    switch (property)
    {
    case wxThunderMaker::PropertyForAnalysis::MaxDelayRaw12C: offset = wxThunderMakerAbi::MaxDelayRawOffset; return true;
    case wxThunderMaker::PropertyForAnalysis::MinDelayRaw128: offset = wxThunderMakerAbi::MinDelayRawOffset; return true;
    default: return false;
    }
}
}

bool wxThunderMaker::ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t& output, ProfileForAnalysis profile)
{
    std::size_t offset = 0;
    if (!Admit(before, profile) || !Offset(property, offset)) return false;
    output = ReadWord(before, offset);
    return true;
}
bool wxThunderMaker::WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    std::size_t offset = 0;
    if (!Admit(before, profile) || !Offset(property, offset)) return false;
    auto staged = before;
    WriteWord(staged, offset, rawBits);
    output = staged;
    return true;
}
bool wxThunderMaker::ComposeRawPropertiesForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawMaxDelay, std::uint32_t rawMinDelay, ComposedPropertiesForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedPropertiesForAnalysis staged{};
    auto state = before;
    const auto maxDelay = PropertyForAnalysis::MaxDelayRaw12C;
    const auto minDelay = PropertyForAnalysis::MinDelayRaw128;
    ReadRawPropertyForAnalysis(state, maxDelay, staged.rawGetterResults[0], profile);
    ReadRawPropertyForAnalysis(state, minDelay, staged.rawGetterResults[1], profile);
    WriteRawPropertyForAnalysis(state, maxDelay, rawMaxDelay, state, profile);
    staged.afterMaxSet = state;
    ReadRawPropertyForAnalysis(state, maxDelay, staged.rawGetterResults[2], profile);
    ReadRawPropertyForAnalysis(state, minDelay, staged.rawGetterResults[3], profile);
    WriteRawPropertyForAnalysis(state, minDelay, rawMinDelay, state, profile);
    staged.afterMinSet = state;
    ReadRawPropertyForAnalysis(state, maxDelay, staged.rawGetterResults[4], profile);
    ReadRawPropertyForAnalysis(state, minDelay, staged.rawGetterResults[5], profile);
    output = staged;
    return true;
}
