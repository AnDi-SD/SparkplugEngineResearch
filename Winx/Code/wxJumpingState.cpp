#include "wxJumpingState.h"

#include "wxJumpingStateOperationsForAnalysis.h"

namespace winx::reconstruction
{
    std::uint32_t wxJumpingState::EvaluateTargetModeForAnalysis(std::uint32_t rawTarget) noexcept
    {
        return jumping_state_operations::Permission38(rawTarget);
    }
}
