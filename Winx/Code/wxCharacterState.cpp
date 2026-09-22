#include "wxCharacterState.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using sparkplug::reconstruction::spBaseObject;
    using sparkplug::reconstruction::spCloneManager;
    using sparkplug::reconstruction::spRTTIManager;
    using sparkplug::reconstruction::spRTTIRecord;

    namespace
    {
        std::unique_ptr<spBaseObject> CreateCharacterState()
        {
            return std::make_unique<wxCharacterState>();
        }

        const spRTTIRecord CharacterStateRecord{
            wxCharacterState::ClassID,
            spBaseObject::ClassID,
            "wxCharacterState",
            &spBaseObject::StaticRTTI(),
            &CreateCharacterState,
            nullptr,
        };

        const bool CharacterStateRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(
                CharacterStateRecord);
    }

    wxCharacterState::wxCharacterState() noexcept = default;
    wxCharacterState::~wxCharacterState() = default;

    const spRTTIRecord& wxCharacterState::StaticRTTI() noexcept
    {
        (void)CharacterStateRegistered;
        return CharacterStateRecord;
    }

    std::unique_ptr<spBaseObject> wxCharacterState::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxCharacterState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        // Native slot 0x14 is the empty spBaseObject copy slot. Runtime state
        // remains at constructor defaults in the clone.
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    const spRTTIRecord& wxCharacterState::vfunc_18() const noexcept
    {
        return CharacterStateRecord;
    }

    bool wxCharacterState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC 512FA0 / PS2 2C9050: release first, then virtual slot 0x30.
        ReleasePendingFromState();
        vfunc_30(request);
        return true;
    }

    bool wxCharacterState::vfunc_20(wxAnimationRequestForAnalysis&)
    {
        ReleasePendingFromState();
        return true;
    }

    void wxCharacterState::vfunc_24() { ReleasePendingFromState(); }
    void wxCharacterState::vfunc_28(wxAnimationRequestForAnalysis&)
    {
        ReleasePendingFromState();
        pendingHandle_ = nullptr;
    }
    void wxCharacterState::vfunc_2C(wxAnimationRequestForAnalysis& request)
    {
        ReleasePendingFromState();
        vfunc_30(request);
    }
    void wxCharacterState::vfunc_30(wxAnimationRequestForAnalysis&) {}

    bool wxCharacterState::vfunc_34(
        const wxAnimationRequestForAnalysis&) const noexcept
    {
        return true;
    }

    bool wxCharacterState::vfunc_38(
        const wxAnimationRequestForAnalysis&) const noexcept
    {
        return false;
    }

    void wxCharacterState::vfunc_3C(wxAnimationRequestForAnalysis&) noexcept {}

    void wxCharacterState::vfunc_40_ResetForAnalysis() noexcept
    {
        // PC 0x00513140 and PS2 0x002C8AC0 restore these observed fields.
        transitionFlag1C_ = true;
        transitionFlag1D_ = true;
        transitionFlag1E_ = true;
        pendingHandle_ = nullptr;
        field28_ = nullptr;
        field2C_ = false;
        // The native zero vector at PC 7600E0 / PS2 476F50.
        resetValues_ = {};
    }

    void wxCharacterState::SetBindingsForAnalysis(void* const owner,
        void* const completionConsumer, wxCharacterStateHost* const host) noexcept
    {
        owner_ = owner;
        completionConsumer_ = completionConsumer;
        host_ = host;
    }

    void wxCharacterState::SetTransitionFlagsForAnalysis(const bool flag1C,
        const bool flag1D, const bool flag1E, const bool flag1F,
        const bool flag20) noexcept
    {
        transitionFlag1C_ = flag1C;
        transitionFlag1D_ = flag1D;
        transitionFlag1E_ = flag1E;
        transitionFlag1F_ = flag1F;
        transitionFlag20_ = flag20;
    }

    void wxCharacterState::SetPendingHandleForAnalysis(void* const handle) noexcept
    {
        pendingHandle_ = handle;
    }

    std::uint32_t wxCharacterState::GetStateSelectorForAnalysis() const noexcept
    {
        return stateSelector_;
    }

    void* wxCharacterState::GetPendingHandleForAnalysis() const noexcept
    {
        return pendingHandle_;
    }

    void wxCharacterState::SetStateSelectorForConstruction(
        const std::uint32_t selector) noexcept
    {
        stateSelector_ = selector;
    }

    wxCharacterStateHost& wxCharacterState::RequireHostForAnalysis() const
    {
        if (host_ == nullptr)
        {
            throw std::logic_error("wxCharacterState requires a host binding for this operation");
        }
        return *host_;
    }

    void* wxCharacterState::GetOwnerForAnalysis() const noexcept
    {
        return owner_;
    }

    void* wxCharacterState::GetCompletionConsumerForAnalysis() const noexcept
    {
        return completionConsumer_;
    }

    void wxCharacterState::SetPendingHandleFromState(void* const handle) noexcept
    {
        pendingHandle_ = handle;
    }

    void wxCharacterState::ClearOwnerActionControlFromState()
    {
        RequireHostForAnalysis().ClearOwnerActionControlForAnalysis(owner_);
    }

    void wxCharacterState::ReleasePendingFromState(const bool forceStop)
    {
        // PC 512E40 / PS2 2C8D60, also inlined in the base state hooks.
        if (pendingHandle_ == nullptr)
        {
            return;
        }
        auto& host = RequireHostForAnalysis();
        if (host.OwnerPredicateForAnalysis(owner_) && !forceStop)
        {
            host.FadeAnimationForAnalysis(completionConsumer_, pendingHandle_, 0.4f);
        }
        else
        {
            host.StopAnimationForAnalysis(completionConsumer_, pendingHandle_);
        }
        pendingHandle_ = nullptr;
    }

    void wxCharacterState::QueuePendingFromState(void* const handle,
        const bool mode, const bool interrupt)
    {
        // PC 512EA0 / PS2 2C8CC0. This helper does not store +0x24.
        auto& host = RequireHostForAnalysis();
        if (!mode)
        {
            host.ResetCompletionForAnalysis(completionConsumer_, handle);
        }
        const auto fade = host.OwnerPredicateForAnalysis(owner_) ? 2u : 0u;
        host.StartAnimationForAnalysis(completionConsumer_, handle, mode, fade, interrupt);
    }

    std::array<bool, 5> wxCharacterState::GetTransitionFlagsForAnalysis() const noexcept
    {
        return {transitionFlag1C_, transitionFlag1D_, transitionFlag1E_,
            transitionFlag1F_, transitionFlag20_};
    }

    bool wxCharacterState::GetTransitionFlag1C() const noexcept
    {
        return transitionFlag1C_;
    }

    bool wxCharacterState::GetTransitionFlag1E() const noexcept
    {
        return transitionFlag1E_;
    }

    bool wxCharacterState::GetTransitionFlag1F() const noexcept
    {
        return transitionFlag1F_;
    }

    bool wxCharacterState::GetTransitionFlag20() const noexcept
    {
        return transitionFlag20_;
    }

    void wxCharacterState::ClearTransitionFlag1C() noexcept
    {
        transitionFlag1C_ = false;
    }

    void wxCharacterState::ClearTransitionFlag1E() noexcept
    {
        transitionFlag1E_ = false;
    }
}
