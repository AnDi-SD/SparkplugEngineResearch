#include "wxRacingCheckpoint.h"
#include "../Analysis/PC/wxRacingCheckpointAbi.h"

namespace
{
using Prefix = wxRacingCheckpoint::CapturedPrefixForAnalysis;
std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
void WriteWord(Prefix& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t byte = 0; byte < 4; ++byte) bytes[offset + byte] = std::uint8_t(value >> (8 * byte));
}
bool Admit(const Prefix& bytes, wxRacingCheckpoint::ProfileForAnalysis profile)
{
    return profile == wxRacingCheckpoint::ProfileForAnalysis::PcCapturedPrimaryRawDwords128138
        && ReadWord(bytes, 0) == wxRacingCheckpointAbi::CapturedPrimary;
}
bool Offset(wxRacingCheckpoint::PropertyForAnalysis property, std::size_t& offset)
{
    switch (property)
    {
    case wxRacingCheckpoint::PropertyForAnalysis::BonusTimeRaw138: offset = wxRacingCheckpointAbi::BonusTimeRawOffset; return true;
    case wxRacingCheckpoint::PropertyForAnalysis::CheckpointIdRaw128: offset = wxRacingCheckpointAbi::CheckpointIdRawOffset; return true;
    default: return false;
    }
}
}

bool wxRacingCheckpoint::ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t& output, ProfileForAnalysis profile)
{
    std::size_t offset = 0;
    if (!Admit(before, profile) || !Offset(property, offset)) return false;
    output = ReadWord(before, offset);
    return true;
}
bool wxRacingCheckpoint::WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    std::size_t offset = 0;
    if (!Admit(before, profile) || !Offset(property, offset)) return false;
    auto staged = before;
    WriteWord(staged, offset, rawBits);
    output = staged;
    return true;
}
bool wxRacingCheckpoint::ComposeRawPropertiesForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBonusTime, std::uint32_t rawCheckpointId, ComposedPropertiesForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedPropertiesForAnalysis staged{};
    auto state = before;
    const auto bonusTime = PropertyForAnalysis::BonusTimeRaw138;
    const auto checkpointId = PropertyForAnalysis::CheckpointIdRaw128;
    ReadRawPropertyForAnalysis(state, bonusTime, staged.rawGetterResults[0], profile);
    ReadRawPropertyForAnalysis(state, checkpointId, staged.rawGetterResults[1], profile);
    WriteRawPropertyForAnalysis(state, bonusTime, rawBonusTime, state, profile);
    staged.afterBonusTimeSet = state;
    ReadRawPropertyForAnalysis(state, bonusTime, staged.rawGetterResults[2], profile);
    ReadRawPropertyForAnalysis(state, checkpointId, staged.rawGetterResults[3], profile);
    WriteRawPropertyForAnalysis(state, checkpointId, rawCheckpointId, state, profile);
    staged.afterCheckpointIdSet = state;
    ReadRawPropertyForAnalysis(state, bonusTime, staged.rawGetterResults[4], profile);
    ReadRawPropertyForAnalysis(state, checkpointId, staged.rawGetterResults[5], profile);
    output = staged;
    return true;
}
