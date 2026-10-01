#include "wxBasicMovingState.h"

#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;

    namespace
    {
        std::unique_ptr<spBaseObject> CreateBasicMovingState()
        {
            return std::make_unique<wxBasicMovingState>();
        }

        const spRTTIRecord record{wxBasicMovingState::ClassID,
            wxCharacterState::ClassID, "wxBasicMovingState",
            &wxCharacterState::StaticRTTI(), &CreateBasicMovingState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }

    wxBasicMovingState::wxBasicMovingState() noexcept
    {
        SetStateSelectorForConstruction(0);
    }

    const spRTTIRecord& wxBasicMovingState::StaticRTTI() noexcept
    {
        (void)registered;
        return record;
    }

    const spRTTIRecord& wxBasicMovingState::vfunc_18() const noexcept
    {
        return record;
    }

    std::unique_ptr<spBaseObject> wxBasicMovingState::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxBasicMovingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    std::uint32_t wxBasicMovingState::ComposeMovingKeyForAnalysis(
        const std::uint32_t input, const float magnitude,
        const bool flag) noexcept
    {
        auto key = input & 0xFFFF807Fu;
        if (magnitude < 0.2f)
        {
            key &= 0xF07FFF8Fu;
            if (flag) key = (key & 0xF0FFFFFFu) | 0x00800000u;
        }
        else if (magnitude < 0.5f)
            key = (key & 0xF07FFFDFu) | 0x50u;
        else
            key = (key & 0xF0FFFFDFu) | 0x00800050u;
        return key & 0xFF9FFFFFu;
    }

    std::uint32_t wxBasicMovingState::ComposeRandomMovingKeyForAnalysis(
        const std::uint32_t input, const bool belowHalf) noexcept
    {
        const auto key = belowHalf
            ? ((input & 0xFFFFFFBFu) | 0x30u)
            : ((input & 0xFFFFFFCFu) | 0x40u);
        return (key & 0xF01F81FFu) | 0x180u;
    }

    void wxBasicMovingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        if (!movementHost_)
            throw std::logic_error("wxBasicMovingState requires a measured movement binding");
        if (GetStateSelectorForAnalysis() == 9)
        {
            if (GetTransitionFlag1D())
            {
                movementHost_->PrepareRandomMovementForAnalysis(*this);
                ClearTransitionFlag1D();
                request.packedKey = ComposeRandomMovingKeyForAnalysis(
                    request.packedKey,
                    movementHost_->RandomMovementBelowHalfForAnalysis());
                void* const handle = movementHost_->ResolveAnimationForAnalysis(
                    GetOwnerForAnalysis(), request.packedKey);
                if (handle != GetPendingHandleForAnalysis())
                {
                    ReleasePendingFromState();
                    QueuePendingFromState(handle, false, true);
                    SetPendingHandleFromState(handle);
                }
            }
            movementHost_->FinishRandomMovementForAnalysis(*this);
            return;
        }
        void* const owner = GetOwnerForAnalysis();
        const float magnitude = movementHost_->MovementMagnitudeForAnalysis(owner);
        const bool flag = magnitude < 0.2f
            && movementHost_->MovementFlagForAnalysis(owner);
        request.packedKey = ComposeMovingKeyForAnalysis(request.packedKey,
            magnitude, flag);
        void* const handle = movementHost_->ResolveAnimationForAnalysis(
            owner, request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }

    bool wxBasicMovingState::vfunc_34(const std::uint32_t target)
    {
        if (target == 10 || target == 11 || GetStateSelectorForAnalysis() != 9
            || !GetPendingHandleForAnalysis())
            return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
}
