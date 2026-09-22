#include "wxActionState.h"

namespace winx::reconstruction
{
    using sparkplug::reconstruction::spBaseObject;
    using sparkplug::reconstruction::spCloneManager;
    using sparkplug::reconstruction::spRTTIManager;
    using sparkplug::reconstruction::spRTTIRecord;

    namespace
    {
        constexpr std::uint32_t Slot1CMask = 0xF027FF8F;
        constexpr std::uint32_t Slot1CSetBits = 0x00200000;
        constexpr std::uint32_t Slot20Mask = 0xF047FF8F;
        constexpr std::uint32_t Slot20SetBits = 0x00400000;
        constexpr std::uint32_t Slot30Mask = 0xF007FF8F;

        std::unique_ptr<spBaseObject> CreateActionState()
        {
            return std::make_unique<wxActionState>();
        }

        const spRTTIRecord ActionStateRecord{
            wxActionState::ClassID,
            wxCharacterState::ClassID,
            "wxActionState",
            &wxCharacterState::StaticRTTI(),
            &CreateActionState,
            nullptr,
        };

        const bool ActionStateRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(
                ActionStateRecord);
    }

    wxActionState::wxActionState() noexcept
    {
        // PS2 2C6CD0 and the captured PC factory initialize the same base
        // fields, then install this vtable and write selector 25 at +0x10.
        SetStateSelectorForConstruction(StateSelector);
    }

    wxActionState::~wxActionState() = default;

    const spRTTIRecord& wxActionState::StaticRTTI() noexcept
    {
        (void)ActionStateRegistered;
        return ActionStateRecord;
    }

    std::unique_ptr<spBaseObject> wxActionState::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxActionState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        // PC 409A20 dispatches the inherited empty copy slot. PS2 3F32E0
        // corroborates this statically; only PC clone execution is claimed.
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    const spRTTIRecord& wxActionState::vfunc_18() const noexcept
    {
        return ActionStateRecord;
    }

    bool wxActionState::vfunc_1C(
        wxAnimationRequestForAnalysis& request)
    {
        // PC 0x00519000 / PS2 0x002C6A00.
        if (!GetTransitionFlag1F())
        {
            return wxCharacterState::vfunc_1C(request);
        }

        if (!GetTransitionFlag1C())
        {
            if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
                    GetCompletionConsumerForAnalysis(),
                    GetPendingHandleForAnalysis(), true))
            {
                return wxCharacterState::vfunc_1C(request);
            }
            ClearOwnerActionControlFromState();
            return false;
        }

        return StartMaskedTransitionForAnalysis(request, Slot1CMask,
            Slot1CSetBits, true);
    }

    bool wxActionState::vfunc_20(
        wxAnimationRequestForAnalysis& request)
    {
        // PC 0x005190B0 / PS2 0x002C68A0.
        if (!GetTransitionFlag20())
        {
            return wxCharacterState::vfunc_20(request);
        }

        if (!GetTransitionFlag1E())
        {
            if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
                    GetCompletionConsumerForAnalysis(),
                    GetPendingHandleForAnalysis(), true))
            {
                return wxCharacterState::vfunc_20(request);
            }
            ClearOwnerActionControlFromState();
            return false;
        }

        ReleasePendingFromState(false);
        return StartMaskedTransitionForAnalysis(request, Slot20Mask,
            Slot20SetBits, false);
    }

    void wxActionState::vfunc_30(
        wxAnimationRequestForAnalysis& request)
    {
        // PC 0x00518F90 / PS2 0x002C6B50.
        request.packedKey &= Slot30Mask;

        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(
                GetOwnerForAnalysis(), request.packedKey);
        if (handle != GetPendingHandleForAnalysis())
        {
            QueuePendingFromState(handle, true, true);
            SetPendingHandleFromState(handle);
        }
        ClearOwnerActionControlFromState();
    }

    bool wxActionState::StartMaskedTransitionForAnalysis(
        wxAnimationRequestForAnalysis& request, const std::uint32_t mask,
        const std::uint32_t setBits, const bool clearFlag1C)
    {
        request.packedKey = (request.packedKey & mask) | setBits;

        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(
                GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        if (clearFlag1C)
        {
            ClearTransitionFlag1C();
        }
        else
        {
            ClearTransitionFlag1E();
        }
        ClearOwnerActionControlFromState();
        return false;
    }
}
