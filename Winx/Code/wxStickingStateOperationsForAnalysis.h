#pragma once
#include "wxCharacterState.h"
#include "Analysis/Host/wxStickingStateHost.h"
#include <stdexcept>
namespace winx::reconstruction
{
    // Analytical alias for structurally equal original state operations.
    // The two game states retain their own RTTI and direct physical base.
    struct wxStickingStateOperationsForAnalysis final
    {
        static wxStickingStateHost& Host(wxCharacterStateHost& binding)
        {
            auto* host = dynamic_cast<wxStickingStateHost*>(&binding);
            if (!host) throw std::logic_error("sticking state requires a motion/angle/movement host");
            return *host;
        }
        static std::uint32_t Variant(std::uint32_t key, bool enabled) noexcept
        { return enabled ? (key & 0xF0FFFFFFu) | 0x800000u : key & 0xF07FFFFFu; }
        template<bool Right, class State> static bool Enter(State& state, wxAnimationRequestForAnalysis& request)
        {
            if (state.GetTransitionFlag1C())
            {
                request.packedKey = (request.packedKey & (Right ? 0xFFA7804Fu : 0xFFA7803Fu))
                    | (Right ? 0x200040u : 0x200030u);
                auto& host = Host(state.RequireHostForAnalysis());
                const auto word = host.ReadEntryOwnerWordForAnalysis(state.GetOwnerForAnalysis());
                request.packedKey = Variant(request.packedKey, (word & 0xFu) == 5);
                void* const handle = host.ResolveAnimationForAnalysis(state.GetOwnerForAnalysis(), request.packedKey);
                // Original first entry replaces pending without releasing it.
                state.QueuePendingFromState(handle, false, true);
                state.SetPendingHandleFromState(handle);
                state.ClearTransitionFlag1C();
            }
            else if (state.RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
                state.GetCompletionConsumerForAnalysis(), state.GetPendingHandleForAnalysis(), true))
                return state.wxCharacterState::vfunc_1C(request);
            state.ClearOwnerActionControlFromState();
            return false;
        }
        template<bool Right, class State> static bool Exit(State& state, wxAnimationRequestForAnalysis& request)
        {
            if (state.GetTransitionFlag1E())
            {
                state.ReleasePendingFromState();
                auto& host = Host(state.RequireHostForAnalysis());
                if ((host.ReadExitOwnerWordForAnalysis(state.GetOwnerForAnalysis()) & 0xFu) == 5)
                    return state.wxCharacterState::vfunc_20(request);
                state.PrepareMovementFromState(false);
                request.packedKey = (request.packedKey & (Right ? 0xFFDFFFCFu : 0xF05FFFBFu))
                    | (Right ? 0x400040u : 0x400030u);
                if constexpr (Right)
                {
                    const auto word = host.ReadExitOwnerWordForAnalysis(state.GetOwnerForAnalysis());
                    request.packedKey = Variant(request.packedKey, (word & 0x180000u) == 0x100000u);
                }
                void* const handle = host.ResolveAnimationForAnalysis(state.GetOwnerForAnalysis(), request.packedKey);
                state.QueuePendingFromState(handle, false, true);
                state.SetPendingHandleFromState(handle);
                state.ClearTransitionFlag1E();
            }
            else if (state.RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
                state.GetCompletionConsumerForAnalysis(), state.GetPendingHandleForAnalysis(), true))
            {
                state.PrepareMovementFromState(false);
                return state.wxCharacterState::vfunc_20(request);
            }
            state.FinishMovementFromState(false);
            state.ClearOwnerActionControlFromState();
            return false;
        }
        template<bool Right, class State> static void Update(State& state, wxAnimationRequestForAnalysis& request)
        {
            auto& host = Host(state.RequireHostForAnalysis());
            const auto objects = host.CaptureStickingObjectsForAnalysis(state.GetOwnerForAnalysis());
            float angle, motion;
            const bool ps2 = host.MovementNumericProfileForAnalysis() == wxCharacterMovementNumericProfileForAnalysis::PS2Finite;
            if (ps2)
            {
                motion = host.ReadStickingMotionForAnalysis(objects.controlObject);
                angle = host.ReadStickingAngleForAnalysis(objects.angleObject);
            }
            else
            {
                angle = host.ReadStickingAngleForAnalysis(objects.angleObject);
                motion = host.ReadStickingMotionForAnalysis(objects.controlObject);
            }
            if (motion < 0.2f) request.packedKey &= 0xF07FFFFFu;
            else
            {
                // PC multiplies float32 global 740978 by3 in x87 without
                // spilling. This exact product fits binary64. PS2 uses the
                // separate rounded float32 literal4016CBE4.
                const double threshold = ps2 ? static_cast<double>(2.35619449615478515625f)
                    : static_cast<double>(0.785398185253143310546875f) * 3.0;
                const bool boost = Right ? angle <= 0.785398185253143310546875f
                    : static_cast<double>(angle) >= threshold;
                if (boost)
                {
                    host.SetConsumerSpeedForAnalysis(state.GetCompletionConsumerForAnalysis(), 1.3f);
                    request.packedKey = Variant(request.packedKey, true);
                }
            }
            request.packedKey = (request.packedKey & (Right ? 0xFF87FFCFu : 0xFF87FFBFu))
                | (Right ? 0x40u : 0x30u);
            void* const handle = host.ResolveAnimationForAnalysis(state.GetOwnerForAnalysis(), request.packedKey);
            if (handle == state.GetPendingHandleForAnalysis()) return;
            state.ReleasePendingFromState();
            state.QueuePendingFromState(handle, true, true);
            state.SetPendingHandleFromState(handle);
        }
    };
}
