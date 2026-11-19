#include "wxParticleCtrl.h"
#include "../Analysis/PC/wxParticleCtrlAbi.h"

namespace
{
std::uint32_t ReadWord(const wxParticleCtrl::CapturedPrefixForAnalysis& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
}
bool wxParticleCtrl::ReadAnimationFileRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile)
{
    if (profile != ProfileForAnalysis::PcCapturedPrimaryAnimationFileRawDword124
        || ReadWord(before, 0) != wxParticleCtrlAbi::CapturedPrimary) return false;
    const auto raw = ReadWord(before, wxParticleCtrlAbi::AnimationFileRawDwordOffset);
    output = raw;
    return true;
}
