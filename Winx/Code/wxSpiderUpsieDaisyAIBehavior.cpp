#include "wxSpiderUpsieDaisyAIBehavior.h"
#include "../Analysis/PC/wxSpiderUpsieDaisyAIBehaviorAbi.h"

namespace
{
using B = wxSpiderUpsieDaisyAIBehavior;
using Prefix = B::CapturedPrefixForAnalysis;
std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
bool Admit(const Prefix& bytes, B::ProfileForAnalysis profile)
{
    return profile == B::ProfileForAnalysis::PcCapturedPrimaryRawDword134
        && ReadWord(bytes, 0) == wxSpiderUpsieDaisyAIBehaviorAbi::CapturedPrimary;
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
