#include "wxChallengeParams.h"
#include "Analysis/PC/wxChallengeParamsAbi.h"

namespace winx::reconstruction
{
    wxChallengeParams::DefaultsResultForAnalysis wxChallengeParams::ResetParameterDefaultsForAnalysis(
        const CapturedPrefixForAnalysis& before, CapturedPrefixForAnalysis& output,
        const DefaultsProfileForAnalysis profile) noexcept
    {
        namespace Abi = winx::analysis::pc::challenge_params;
        static_assert(sizeof(CapturedPrefixForAnalysis) == Abi::CapturedPrefixBytes);
        if (profile != DefaultsProfileForAnalysis::PcCapturedPrimaryInterior124)
            return DefaultsResultForAnalysis::UnqualifiedProfile;
        const auto primary = std::uint32_t(before[0]) | (std::uint32_t(before[1]) << 8)
            | (std::uint32_t(before[2]) << 16) | (std::uint32_t(before[3]) << 24);
        if (primary != Abi::PrimaryTable) return DefaultsResultForAnalysis::CapturedPrimaryNotAdmitted;
        auto staged = before;
        for (std::size_t index = 0; index != Abi::DefaultWords.size(); ++index)
            for (unsigned byte = 0; byte != 4; ++byte)
                staged[Abi::InteriorOffset + index * 4 + byte] = std::uint8_t(Abi::DefaultWords[index] >> (byte * 8));
        output = staged;
        return DefaultsResultForAnalysis::Completed;
    }
}
