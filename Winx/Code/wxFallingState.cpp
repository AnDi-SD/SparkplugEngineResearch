#include "wxFallingState.h"

namespace winx::reconstruction
{
    std::uint32_t wxFallingState::EvaluateTargetModeForAnalysis(std::uint32_t rawTarget) noexcept
    {
        return rawTarget == 1u ? 1u : 0u;
    }
}
