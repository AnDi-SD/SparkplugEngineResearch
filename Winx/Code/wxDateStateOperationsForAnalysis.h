#pragma once
#include "wxCharacterState.h"
#include "Analysis/Host/wxDateStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    // Analytical alias of shared PC524200. PS2 emits the same operation in
    // each date class. Both platforms pass the owner word by value.
    inline bool ExitDateStateForAnalysis(wxCharacterState& state,
        wxAnimationRequestForAnalysis& request, void* owner, wxCharacterStateHost& binding)
    {
        auto* host = dynamic_cast<wxDateStateHost*>(&binding);
        if (!host) throw std::logic_error("date state requires an exit-notification host");
        void* const ownerWord = host->OwnerField24ForAnalysis(owner);
        host->SendExitNotificationForAnalysis(state, 0x27ED, 0x1D,
            state.GetStateSelectorForAnalysis(), ownerWord);
        return state.wxCharacterState::vfunc_20(request);
    }
}
