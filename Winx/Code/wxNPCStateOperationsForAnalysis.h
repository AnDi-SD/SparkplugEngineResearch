#pragma once
#include "wxCharacterState.h"
#include "Analysis/Host/wxNPCStateHost.h"
#include <optional>
#include <stdexcept>
namespace winx::reconstruction
{
    // Analytical representation of native byte3C and pointer40, not a
    // physical intermediate C++ base. Both platforms clear byte3C; only
    // pointer40 and the byte's padding remain uninitialized at construction.
    struct wxNPCStateFieldsForAnalysis final
    {
        std::uint8_t field3C = 0;
        std::optional<void*> field40;
        [[nodiscard]] std::uint8_t RequireField3C() const noexcept { return field3C; }
    };
    // Shared original PC5A7DD0 entry and the two structurally equal updates.
    // Friend access is through each real derived state, without changing base
    // fields, RTTI or physical inheritance. This type is our analytical alias.
    struct wxNPCStateOperationsForAnalysis final
    {
        template<class State> static bool Enter(State& state, wxAnimationRequestForAnalysis& request)
        {
            auto* host = dynamic_cast<wxNPCStateHost*>(&state.RequireHostForAnalysis());
            if (!host) throw std::logic_error("NPC state requires an owner-field host");
            state.fields_.field40 = host->OwnerEntityField130ForAnalysis(state.GetOwnerForAnalysis());
            state.fields_.field3C = std::uint8_t{0};
            state.vfunc_30(request);
            return state.wxCharacterState::vfunc_1C(request);
        }
        template<class State> static void UpdateModeZero(State& state,
            wxAnimationRequestForAnalysis& request, std::uint32_t mask, std::uint32_t bits)
        {
            request.packedKey = (request.packedKey & mask) | bits;
            void* const handle = state.RequireHostForAnalysis().ResolveAnimationForAnalysis(state.GetOwnerForAnalysis(), request.packedKey);
            if (handle == state.GetPendingHandleForAnalysis()) return;
            state.fields_.field3C = std::uint8_t{1};
            state.ReleasePendingFromState();
            state.QueuePendingFromState(handle, false, true);
            state.SetPendingHandleFromState(handle);
        }
    };
}
