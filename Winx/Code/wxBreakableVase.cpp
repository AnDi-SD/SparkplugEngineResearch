#include "wxBreakableVase.h"
#include "../Analysis/PC/wxBreakableVaseAbi.h"

namespace
{
using B = wxBreakableVase;
using Prefix = B::CapturedPrefixForAnalysis;
std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
bool Admit(const Prefix& bytes, B::ProfileForAnalysis profile)
{
    return profile == B::ProfileForAnalysis::PcCapturedPrimaryRawDword17C
        && ReadWord(bytes, 0) == wxBreakableVaseAbi::CapturedPrimary;
}
}
bool wxBreakableVase::ReadFairyStarsValueRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    output = ReadWord(before, wxBreakableVaseAbi::FairyStarsValueRawOffset);
    return true;
}
bool wxBreakableVase::WriteFairyStarsValueRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    auto staged = before;
    for (std::size_t byte = 0; byte < 4; ++byte)
        staged[wxBreakableVaseAbi::FairyStarsValueRawOffset + byte] = std::uint8_t(rawBits >> (byte * 8));
    output = staged;
    return true;
}
bool wxBreakableVase::ComposeFairyStarsValueRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, ComposedValueForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedValueForAnalysis staged{};
    auto state = before;
    if (!ReadFairyStarsValueRawForAnalysis(state, staged.beforeRaw, profile)
        || !WriteFairyStarsValueRawForAnalysis(state, rawBits, state, profile)
        || !ReadFairyStarsValueRawForAnalysis(state, staged.afterRaw, profile)) return false;
    staged.afterSet = state;
    output = staged;
    return true;
}
