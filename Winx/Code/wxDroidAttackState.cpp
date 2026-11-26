#include "wxDroidAttackState.h"

#include "../Analysis/PC/wxDroidAttackStateAbi.h"

namespace winx::reconstruction
{
    namespace
    {
        namespace Abi = winx::evidence::pc;

        static_assert(Abi::wxDroidAttackStatePermissionByteOffset
            < wxDroidAttackState::CapturedStatePrefixForAnalysis{}.size());
    }

    bool wxDroidAttackState::TryReadPcTransitionPermissionForAnalysis(
        const CapturedStatePrefixForAnalysis& state, std::uint32_t rawTarget,
        std::uint32_t& eax) noexcept
    {
        const auto primary = std::uint32_t(state[0]) | (std::uint32_t(state[1]) << 8)
            | (std::uint32_t(state[2]) << 16) | (std::uint32_t(state[3]) << 24);
        if (primary != Abi::wxDroidAttackStateVTable)
            return false;

        eax = (rawTarget == 10u || state[Abi::wxDroidAttackStatePermissionByteOffset] != 0) ? 1u : 0u;
        return true;
    }
}
