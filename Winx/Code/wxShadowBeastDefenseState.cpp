#include "wxShadowBeastDefenseState.h"
#include "Analysis/Host/wxShadowBeastDefenseStateHost.h"
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxShadowBeastDefenseState>(); }
        const spRTTIRecord record{wxShadowBeastDefenseState::ClassID, wxCharacterState::ClassID,
            "wxShadowBeastDefenseState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxShadowBeastDefenseStateHost& RequireDefenseHost(wxCharacterStateHost& host)
        {
            auto* result = dynamic_cast<wxShadowBeastDefenseStateHost*>(&host);
            if (!result) throw std::logic_error("wxShadowBeastDefenseState requires a notification/controller host");
            return *result;
        }
        void Notify(wxShadowBeastDefenseStateHost& host, wxShadowBeastDefenseState& state, void* owner, std::uint32_t code)
        {
            void* const receiver = host.OwnerField24ForAnalysis(owner);
            if (receiver) host.SendDefenseNotificationForAnalysis(receiver, state, code);
        }
    }
    wxShadowBeastDefenseState::wxShadowBeastDefenseState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxShadowBeastDefenseState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxShadowBeastDefenseState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxShadowBeastDefenseState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxShadowBeastDefenseState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void* wxShadowBeastDefenseState::RequireControllerForAnalysis() const
    {
        if (!field40_) throw std::logic_error("defense controller word40 is uninitialized until externally bound");
        return *field40_;
    }
    bool wxShadowBeastDefenseState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC5233C0 / PS230FA30. Own byte clears AFTER playback/once flag.
        if (GetTransitionFlag1C())
        {
            ReleasePendingFromState();
            request.packedKey = (request.packedKey & 0xF0200180u) | 0x200180u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            ClearTransitionFlag1C();
            field3C_ = 0;
        }
        else if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            auto& host = RequireDefenseHost(RequireHostForAnalysis());
            Notify(host, *this, GetOwnerForAnalysis(), 0x27D1);
            host.InvokeDefenseControllerForAnalysis(RequireControllerForAnalysis(), 1, 1);
            return wxCharacterState::vfunc_1C(request);
        }
        ClearOwnerActionControlFromState();
        return false;
    }
    bool wxShadowBeastDefenseState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        // PC521830 / PS230F6C0. Notification/controller happen before release.
        if (GetTransitionFlag1E())
        {
            auto& host = RequireDefenseHost(RequireHostForAnalysis());
            Notify(host, *this, GetOwnerForAnalysis(), 0x27D2);
            host.InvokeDefenseControllerForAnalysis(RequireControllerForAnalysis(), 0, 1);
            ReleasePendingFromState();
            request.packedKey = (request.packedKey & 0xF0400180u) | 0x400180u;
            void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            ClearTransitionFlag1E();
        }
        else if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
            return wxCharacterState::vfunc_20(request);
        ClearOwnerActionControlFromState();
        return false;
    }
    void wxShadowBeastDefenseState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC5234A0 / PS230F8D0: no own-field changes and no completion query.
        request.packedKey = (request.packedKey & 0xF0000180u) | 0x180u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
}
