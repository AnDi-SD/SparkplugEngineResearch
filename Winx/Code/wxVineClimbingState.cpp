#include "wxVineClimbingState.h"
#include "Analysis/Host/wxVineClimbingStateHost.h"
#include "Analysis/PC/wxVineClimbingNumeric.h"
#include <cmath>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxVineClimbingState>(); }
        const spRTTIRecord record{wxVineClimbingState::ClassID, wxCharacterState::ClassID,
            "wxVineClimbingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxVineClimbingStateHost& Host(wxCharacterStateHost& host)
        {
            auto* result = dynamic_cast<wxVineClimbingStateHost*>(&host);
            if (!result) throw std::logic_error("VineClimbing requires an actor/node host");
            return *result;
        }
        bool PS2(wxVineClimbingStateHost& host)
        { return host.MovementNumericProfileForAnalysis() == wxCharacterMovementNumericProfileForAnalysis::PS2Finite; }
        void SetDirection(wxAnimationRequestForAnalysis& request, std::uint32_t direction)
        { request.packedKey = (request.packedKey & 0xFFFFFF8Fu) | direction; }
    }
    wxVineClimbingState::wxVineClimbingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxVineClimbingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxVineClimbingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxVineClimbingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxVineClimbingState>(); manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxVineClimbingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        auto& host = Host(RequireHostForAnalysis());
        const auto angles = host.OwnerAnglesForAnalysis(GetOwnerForAnalysis());
        const float motion = host.OwnerMotionForAnalysis(GetOwnerForAnalysis());
        constexpr float low = 0.78539818525314331055f;
        const double high = PS2(host) ? double(2.35619449615478515625f) : double(low) * 3.0;
        if (motion < 0.2f) SetDirection(request, 0);
        else if (angles[0] <= low) SetDirection(request, 0x10);
        else if (double(angles[0]) >= high) SetDirection(request, 0x20);
        else if (double(angles[1]) >= high) SetDirection(request, 0x30);
        else if (angles[1] <= low) SetDirection(request, 0x40);
        else
        {
            SetDirection(request, 0);
            ClearOwnerActionControlFromState();
        }
        request.packedKey &= 0xF007FFFFu;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState(); QueuePendingFromState(handle, true, true); SetPendingHandleFromState(handle);
    }
    bool wxVineClimbingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto& host = Host(RequireHostForAnalysis());
        if (GetTransitionFlag1C())
        {
            PrepareMovementFromState(false);
            request.packedKey = (request.packedKey & 0xFFA7807Fu) | 0x200000u;
            if (host.OwnerControlByte50ForAnalysis(GetOwnerForAnalysis()))
            {
                SetDirection(request, 0x20);
                host.PrepareTurnForAnalysis(*this);
                host.WriteEntityControlFlagForAnalysis(host.OwnerEntityControlForAnalysis(GetOwnerForAnalysis()), 1);
                host.RotateClimbingNodeForAnalysis(host.OwnerMovementTargetForAnalysis(GetOwnerForAnalysis()), 3.14159274101257324219f);
                host.InvokeClimbingNodeForAnalysis(host.OwnerMovementTargetForAnalysis(GetOwnerForAnalysis()), 0);
            }
            else SetDirection(request, 0x10);
            request.packedKey &= 0xF07FFFFFu;
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true); SetPendingHandleFromState(handle); ClearTransitionFlag1C();
        }
        else if (host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            host.WriteEntityControlFlagForAnalysis(host.OwnerEntityControlForAnalysis(GetOwnerForAnalysis()), 0);
            PrepareMovementFromState(false);
            return wxCharacterState::vfunc_1C(request);
        }
        FinishMovementFromState(true); ClearOwnerActionControlFromState(); return false;
    }
    bool wxVineClimbingState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        auto& host = Host(RequireHostForAnalysis());
        if (GetTransitionFlag1E())
        {
            ReleasePendingFromState(); PrepareMovementFromState(false);
            host.WriteEntityControlFlagForAnalysis(host.OwnerEntityControlForAnalysis(GetOwnerForAnalysis()), 1);
            request.packedKey = (request.packedKey & 0xFFDFFFFFu) | 0x400000u;
            if (host.OwnerControlByte50ForAnalysis(GetOwnerForAnalysis())) SetDirection(request, 0x10);
            else
            {
                void* const control = host.OwnerEntityControlForAnalysis(GetOwnerForAnalysis());
                const auto height = host.ExitHeightsForAnalysis(control);
                if (PS2(host) && (!std::isfinite(height[0]) || !std::isfinite(height[1])))
                    throw std::logic_error("PS2 climbing exit requires finite qualified heights");
                const bool earlyExit = PS2(host) ? float(height[0] - height[1]) > 50.0f
                    : winx::analysis::pc::HeightDifferenceAbove50ForAnalysis(height[0], height[1]);
                if (earlyExit)
                {
                    host.WriteEntityControlFlagForAnalysis(control, 0);
                    host.ResetEntityControlForAnalysis(host.OwnerEntityControlForAnalysis(GetOwnerForAnalysis()));
                    void* const node = host.OwnerMovementTargetForAnalysis(GetOwnerForAnalysis());
                    if (PS2(host)) host.MovePS2ExitNodeForAnalysis(node, -20.0f);
                    else
                    {
                        auto delta = host.ReadExitNodeAxisForAnalysis(node);
                        for (auto& value : delta) value = static_cast<float>(double(value) * -20.0);
                        host.TranslateExitNodeForAnalysis(node, delta);
                    }
                    host.InvokeClimbingNodeForAnalysis(node, 0);
                    // Native early exit leaves flag1E and the action word alone.
                    return wxCharacterState::vfunc_20(request);
                }
                SetDirection(request, 0x20);
            }
            request.packedKey &= 0xF07FFFFFu;
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true); SetPendingHandleFromState(handle); ClearTransitionFlag1E();
        }
        else if (host.IsPendingAnimationCompleteForAnalysis(GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            host.WriteEntityControlFlagForAnalysis(host.OwnerEntityControlForAnalysis(GetOwnerForAnalysis()), 0);
            PrepareMovementFromState(false); return wxCharacterState::vfunc_20(request);
        }
        FinishMovementFromState(false); ClearOwnerActionControlFromState(); return false;
    }
}
