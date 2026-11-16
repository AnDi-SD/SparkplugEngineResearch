#include "wxSwampGazBubble.h"
#include "../Analysis/PC/wxSwampGazBubbleAbi.h"

namespace
{
using B = wxSwampGazBubble;
using Prefix = B::CapturedPrefixForAnalysis;
std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
bool Admit(const Prefix& bytes, B::ProfileForAnalysis profile)
{
    return profile == B::ProfileForAnalysis::PcCapturedPrimaryRawDword14C
        && ReadWord(bytes, 0) == wxSwampGazBubbleAbi::CapturedPrimary;
}
}
bool wxSwampGazBubble::ReadDamageInflictedRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    output = ReadWord(before, wxSwampGazBubbleAbi::DamageInflictedRawOffset);
    return true;
}
bool wxSwampGazBubble::WriteDamageInflictedRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    auto staged = before;
    for (std::size_t byte = 0; byte < 4; ++byte)
        staged[wxSwampGazBubbleAbi::DamageInflictedRawOffset + byte] = std::uint8_t(rawBits >> (byte * 8));
    output = staged;
    return true;
}
bool wxSwampGazBubble::ComposeDamageInflictedRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, ComposedDamageForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedDamageForAnalysis staged{};
    auto state = before;
    if (!ReadDamageInflictedRawForAnalysis(state, staged.beforeRaw, profile)
        || !WriteDamageInflictedRawForAnalysis(state, rawBits, state, profile)
        || !ReadDamageInflictedRawForAnalysis(state, staged.afterRaw, profile)) return false;
    staged.afterSet = state;
    output = staged;
    return true;
}
