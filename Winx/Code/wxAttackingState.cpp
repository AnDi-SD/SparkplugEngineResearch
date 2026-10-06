#include "wxAttackingState.h"

#include <stdexcept>

namespace winx::reconstruction
{
    using sparkplug::reconstruction::spBaseObject;
    using sparkplug::reconstruction::spCloneManager;
    using sparkplug::reconstruction::spRTTIManager;
    using sparkplug::reconstruction::spRTTIRecord;

    namespace
    {
        std::unique_ptr<spBaseObject> CreateAttackingState()
        {
            return std::make_unique<wxAttackingState>();
        }

        const spRTTIRecord AttackingStateRecord{
            wxAttackingState::ClassID,
            wxCharacterState::ClassID,
            "wxAttackingState",
            &wxCharacterState::StaticRTTI(),
            &CreateAttackingState,
            nullptr,
        };

        const bool AttackingStateRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(
                AttackingStateRecord);
    }

    wxAttackingState::wxAttackingState() noexcept
    {
        SetStateSelectorForConstruction(StateSelector);
    }

    wxAttackingState::~wxAttackingState() = default;

    const spRTTIRecord& wxAttackingState::StaticRTTI() noexcept
    {
        (void)AttackingStateRegistered;
        return AttackingStateRecord;
    }

    std::unique_ptr<spBaseObject> wxAttackingState::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxAttackingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        // PC 409250 / PS2 3F4BE0 use the inherited empty copy slot.
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    const spRTTIRecord& wxAttackingState::vfunc_18() const noexcept
    {
        return AttackingStateRecord;
    }

    wxAttackingStateHost& wxAttackingState::RequireAttackHostForAnalysis() const
    {
        auto* host = dynamic_cast<wxAttackingStateHost*>(&RequireHostForAnalysis());
        if (host == nullptr)
        {
            throw std::logic_error("wxAttackingState requires an attack host binding");
        }
        return *host;
    }

    bool wxAttackingState::CanSkipTransitionForAnalysis(
        const std::uint32_t key)
    {
        // PC 513EA0 / PS2 2C6D10. This is a qualified helper, not vtable
        // slot 0x34 or 0x38. Its true result enters the inherited transition.
        if ((key & 0x00180000) != 0)
        {
            return true;
        }
        auto& host = RequireAttackHostForAnalysis();
        const auto kind = key & 0xF;
        if (kind == 2)
        {
            return !(host.GetAttackMotionForAnalysis(GetOwnerForAnalysis()) < 0.2f)
                || (key & 0x70) != 0;
        }
        if (kind != 0 && kind != 3)
        {
            return false;
        }
        if (host.GetAttackMotionForAnalysis(GetOwnerForAnalysis()) > 0.2f
            || (key & 0x70) == 0x50)
        {
            return true;
        }
        if (host.GetOwnerActionCountForAnalysis(GetOwnerForAnalysis()) == 0)
        {
            return false;
        }
        const auto first = host.GetFirstOwnerActionForAnalysis(GetOwnerForAnalysis());
        return first == 1 || first == 0x12;
    }

    std::uint32_t wxAttackingState::ApplyGlobalAttackModeForAnalysis(
        std::uint32_t key)
    {
        // PC global +0x512 and predicate 4E64F0; PS2 uses the same gate.
        if (RequireAttackHostForAnalysis().IsAttackOverrideActiveForAnalysis())
        {
            return (key & 0xF0FFFFFF) | 0x00800000;
        }
        return key & 0xF07FFFFF;
    }

    void wxAttackingState::StartTransitionForAnalysis(
        wxAnimationRequestForAnalysis& request, const std::uint32_t clearMask,
        const std::uint32_t setBits)
    {
        ReleasePendingFromState(false);
        request.packedKey = ApplyGlobalAttackModeForAnalysis(
            (request.packedKey & clearMask) | setBits);
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(
            GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
    }

    bool wxAttackingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC 514210 / PS2 2C7050. The override enters the base hook even
        // when +0x1c is still set.
        if (RequireAttackHostForAnalysis().IsAttackOverrideActiveForAnalysis())
        {
            eventFlag_ = false;
            return wxCharacterState::vfunc_1C(request);
        }
        if (!GetTransitionFlag1C())
        {
            if (!RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
                    GetCompletionConsumerForAnalysis(),
                    GetPendingHandleForAnalysis(), true))
            {
                return false;
            }
            return wxCharacterState::vfunc_1C(request);
        }
        eventFlag_ = false;
        if (CanSkipTransitionForAnalysis(request.packedKey))
        {
            return wxCharacterState::vfunc_1C(request);
        }
        StartTransitionForAnalysis(request, 0xFFBFFFFF, 0x00200000);
        eventFlag_ = false;
        ClearTransitionFlag1C();
        return false;
    }

    bool wxAttackingState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        // PC 514340 / PS2 2C6E50.
        if (RequireAttackHostForAnalysis().IsAttackOverrideActiveForAnalysis())
        {
            return wxCharacterState::vfunc_20(request);
        }
        if (!GetTransitionFlag1E())
        {
            if (!RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
                    GetCompletionConsumerForAnalysis(),
                    GetPendingHandleForAnalysis(), true))
            {
                return false;
            }
            return wxCharacterState::vfunc_20(request);
        }
        if (CanSkipTransitionForAnalysis(request.packedKey))
        {
            return wxCharacterState::vfunc_20(request);
        }
        StartTransitionForAnalysis(request, 0xFFDFFFFF, 0x00400000);
        ClearTransitionFlag1E();
        return false;
    }

    void wxAttackingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC 5140A0 / PS2 2C7260. PS2 clears the whole +0x70 group before
        // setting a variant; the PC masks preserve only bits then set them
        // to the same final value.
        auto& host = RequireAttackHostForAnalysis();
        const float motion = host.GetAttackMotionForAnalysis(GetOwnerForAnalysis());
        auto key = request.packedKey;
        if (motion < 0.2f)
        {
            key &= 0xFFFFFF8F;
        }
        else if ((key & 0xF) != 2)
        {
            key = (key & 0xFFFFFFDF) | 0x50;
        }
        else
        {
            const float angle = host.GetAttackAngleForAnalysis(GetOwnerForAnalysis());
            constexpr float QuarterTurn = 0.7853981852531433f;
            if (angle > -QuarterTurn && angle < QuarterTurn)
            {
                key = (key & 0xFFFFFF9F) | 0x10;
            }
            else if (angle < -3.0f * QuarterTurn
                || angle > 3.0f * QuarterTurn)
            {
                key = (key & 0xFFFFFFAF) | 0x20;
            }
            else if (angle > 0.0f)
            {
                key = (key & 0xFFFFFFCF) | 0x40;
            }
            else
            {
                key = (key & 0xFFFFFFBF) | 0x30;
            }
        }
        request.packedKey = ApplyGlobalAttackModeForAnalysis(key) & 0xFF9FFFFF;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(
            GetOwnerForAnalysis(), request.packedKey);
        if (handle != GetPendingHandleForAnalysis())
        {
            ReleasePendingFromState(false);
            QueuePendingFromState(handle, true, true);
            SetPendingHandleFromState(handle);
        }
    }

    bool wxAttackingState::vfunc_34(const std::uint32_t code)
    {
        // PC 513E70 / PS2 2C76F0.
        if (code == 1
            && (RequireAttackHostForAnalysis().GetOwnerModeForAnalysis(
                    GetOwnerForAnalysis()) & 0xF) == 3)
        {
            return false;
        }
        return eventFlag_;
    }

    std::uint32_t wxAttackingState::vfunc_38(const std::uint32_t code) const noexcept
    {
        // PC 5144F0 / PS2 2C76A0: explicit false cases in the code table.
        switch (code)
        {
        case 0: case 4: case 8: case 10: case 18: return false;
        default: return true;
        }
    }

    void wxAttackingState::vfunc_3C(const void* event)
    {
        // PC 513FA0 / PS2 2C7510. Both names are exact null-terminated
        // event strings; their external event-object chain stays in the host.
        auto& host = RequireAttackHostForAnalysis();
        const auto name = host.GetEventNameForAnalysis(event);
        if (name == "event_orb")
        {
            eventFlag_ = true;
            if (host.IsOrbImmediateForAnalysis(GetOwnerForAnalysis())
                || host.CanTriggerOrbForAnalysis(GetOwnerForAnalysis(), 1.0f))
            {
                host.TriggerEventForAnalysis(GetOwnerForAnalysis(), 0);
            }
        }
        else if (name == "event_snowball")
        {
            eventFlag_ = true;
            const auto count = host.GetSnowballCountForAnalysis();
            if (count > 0)
            {
                host.TriggerEventForAnalysis(GetOwnerForAnalysis(), 6);
                host.SetSnowballCountForAnalysis(count - 1);
            }
        }
    }

    bool wxAttackingState::GetEventFlagForAnalysis() const noexcept
    {
        return eventFlag_;
    }
}
