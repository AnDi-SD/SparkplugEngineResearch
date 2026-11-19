#include "wxUpsieDaisyGuardAIBehavior.h"
#include "../Analysis/PC/wxUpsieDaisyGuardAIBehaviorAbi.h"

namespace
{
using B = wxUpsieDaisyGuardAIBehavior;
using Prefix = B::CapturedPrefixForAnalysis;
std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
bool Admit(const Prefix& bytes, B::ProfileForAnalysis profile)
{
    return profile == B::ProfileForAnalysis::PcCapturedPrimaryRawDword184
        && ReadWord(bytes, 0) == wxUpsieDaisyGuardAIBehaviorAbi::CapturedPrimary;
}
}
bool wxUpsieDaisyGuardAIBehavior::ReadGuardNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    output = ReadWord(before, wxUpsieDaisyGuardAIBehaviorAbi::GuardNumRawOffset);
    return true;
}
bool wxUpsieDaisyGuardAIBehavior::WriteGuardNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    auto staged = before;
    for (std::size_t byte = 0; byte < 4; ++byte)
        staged[wxUpsieDaisyGuardAIBehaviorAbi::GuardNumRawOffset + byte] = std::uint8_t(rawBits >> (byte * 8));
    output = staged;
    return true;
}
bool wxUpsieDaisyGuardAIBehavior::ComposeGuardNumRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, ComposedGuardNumForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedGuardNumForAnalysis staged{};
    auto state = before;
    if (!ReadGuardNumRawForAnalysis(state, staged.beforeRaw, profile)
        || !WriteGuardNumRawForAnalysis(state, rawBits, state, profile)
        || !ReadGuardNumRawForAnalysis(state, staged.afterRaw, profile)) return false;
    staged.afterSet = state;
    output = staged;
    return true;
}
