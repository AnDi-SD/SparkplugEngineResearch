#include "wxTrollMovingState.h"

#include "Analysis/PC/wxMovingStateAirEventsAbi.h"

namespace winx::reconstruction
{
    bool wxTrollMovingState::TryApplyAirEventForAnalysis(MovingStatePrefixForAnalysis& state,
        const MovingStateAirEventCaptureForAnalysis& event) noexcept
    {
        namespace Abi = winx::evidence::pc;
        return moving_state_air_event_operations::TryApply<Abi::wxTrollMovingStateAirEventByteOffset>(
            state, Abi::wxTrollMovingStateVTable, event);
    }
}
