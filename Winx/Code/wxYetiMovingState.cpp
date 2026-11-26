#include "wxYetiMovingState.h"

#include "Analysis/PC/wxMovingStateAirEventsAbi.h"

namespace winx::reconstruction
{
    bool wxYetiMovingState::TryApplyAirEventForAnalysis(MovingStatePrefixForAnalysis& state,
        const MovingStateAirEventCaptureForAnalysis& event) noexcept
    {
        namespace Abi = winx::evidence::pc;
        return moving_state_air_event_operations::TryApply<Abi::wxYetiMovingStateAirEventByteOffset>(
            state, Abi::wxYetiMovingStateVTable, event);
    }
}
