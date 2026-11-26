#include "wxGettingCollectibleState.h"

namespace winx::reconstruction
{
    std::uint32_t wxGettingCollectibleState::EvaluateTargetModeForAnalysis(std::uint32_t rawTarget) noexcept
    {
        return rawTarget == 2u ? 1u : 0u;
    }
}
