#include "wxSparxBehavior.h"
#include "../Analysis/PC/wxSparxBehaviorAbi.h"

namespace
{
using B = wxSparxBehavior;
using Prefix = B::CapturedPrefixForAnalysis;
bool Admit(const Prefix& bytes, B::ProfileForAnalysis profile)
{
    const auto primary = std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
        | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
    return profile == B::ProfileForAnalysis::PcCapturedPrimaryTestAnimationRawByte128
        && primary == wxSparxBehaviorAbi::CapturedPrimary;
}
}
bool wxSparxBehavior::ReadTestAnimationByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint8_t& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    output = before[wxSparxBehaviorAbi::TestAnimationRawByteOffset];
    return true;
}
bool wxSparxBehavior::WriteTestAnimationLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgumentWord, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    auto staged = before;
    staged[wxSparxBehaviorAbi::TestAnimationRawByteOffset] = std::uint8_t(rawArgumentWord & 0xFFu);
    output = staged;
    return true;
}
bool wxSparxBehavior::ComposeTestAnimationLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgumentWord, ComposedTestAnimationByteForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedTestAnimationByteForAnalysis staged{};
    auto state = before;
    if (!ReadTestAnimationByteForAnalysis(state, staged.beforeRaw, profile)
        || !WriteTestAnimationLowByteForAnalysis(state, rawArgumentWord, state, profile)
        || !ReadTestAnimationByteForAnalysis(state, staged.afterRaw, profile)) return false;
    staged.afterSet = state;
    output = staged;
    return true;
}
