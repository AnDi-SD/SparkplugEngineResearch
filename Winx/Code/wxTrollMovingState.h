#pragma once

#include "wxMovingStateAirEventOperationsForAnalysis.h"

namespace winx::reconstruction
{
    class wxTrollMovingState
    {
    public:
        // A true result admits the supplied capture for the PC slot +0x3C operation.
        [[nodiscard]] static bool TryApplyAirEventForAnalysis(MovingStatePrefixForAnalysis& state,
            const MovingStateAirEventCaptureForAnalysis& event) noexcept;
    };
}
