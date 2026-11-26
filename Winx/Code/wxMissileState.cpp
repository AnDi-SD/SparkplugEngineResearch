#include "wxMissileState.h"

#include "Analysis/PC/wxMissileStateAbi.h"

namespace winx::reconstruction
{
    std::optional<std::uint8_t> wxMissileState::ReadPcPermissionByteForAnalysis(
        const CapturedStatePrefixForAnalysis& state, std::uint32_t) noexcept
    {
        namespace Abi = winx::evidence::pc;
        static_assert(CapturedStatePrefixForAnalysis{}.size() == Abi::wxMissileStateCaptureBytes);
        static_assert(Abi::wxMissileStatePermissionByteOffset < Abi::wxMissileStateCaptureBytes);
        const auto primary = std::uint32_t(state[0]) | (std::uint32_t(state[1]) << 8)
            | (std::uint32_t(state[2]) << 16) | (std::uint32_t(state[3]) << 24);
        if (primary != Abi::wxMissileStatePrimary)
            return std::nullopt;
        return state[Abi::wxMissileStatePermissionByteOffset];
    }
}
