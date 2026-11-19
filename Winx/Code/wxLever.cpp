#include "wxLever.h"
#include "../Analysis/PC/wxLeverAbi.h"

namespace
{
std::uint32_t ReadWord(const wxLever::CapturedPrefixForAnalysis& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
}
bool wxLever::ReadActionTokenRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile)
{
    if (profile != ProfileForAnalysis::PcCapturedPrimaryActionRawDword124
        || ReadWord(before, 0) != wxLeverAbi::CapturedPrimary) return false;
    const auto raw = ReadWord(before, wxLeverAbi::ActionRawDwordOffset);
    output = raw;
    return true;
}
