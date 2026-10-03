#include "wxMinotaurDefenseState.h"
#include "Analysis/Host/wxMinotaurDefenseStateHost.h"
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxMinotaurDefenseState>(); }
        const spRTTIRecord record{wxMinotaurDefenseState::ClassID, wxCharacterState::ClassID,
            "wxMinotaurDefenseState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxMinotaurDefenseStateHost& RequireDefenseHost(wxCharacterStateHost& host)
        {
            auto* result = dynamic_cast<wxMinotaurDefenseStateHost*>(&host);
            if (!result) throw std::logic_error("wxMinotaurDefenseState requires a notification/controller host");
            return *result;
        }
        void Notify(wxMinotaurDefenseStateHost& host, wxMinotaurDefenseState& state, void* owner, std::uint32_t code)
        {
            void* const receiver = host.OwnerField24ForAnalysis(owner);
            if (receiver) host.SendDefenseNotificationForAnalysis(receiver, state, code);
        }
    }
    wxMinotaurDefenseState::wxMinotaurDefenseState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxMinotaurDefenseState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxMinotaurDefenseState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxMinotaurDefenseState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxMinotaurDefenseState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void* wxMinotaurDefenseState::RequireControllerForAnalysis() const
    {
        if (!field40_) throw std::logic_error("defense controller word40 is uninitialized until externally bound");
        return *field40_;
    }
    bool wxMinotaurDefenseState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC5233C0 / PS2304E50. Own byte clears AFTER playback/once flag.
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
    bool wxMinotaurDefenseState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        // PC521830 / PS2304A30. Notification/controller happen before release.
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
    void wxMinotaurDefenseState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC521750 / PS2304C40. Capture the control object before reading
        // motion; the first write precedes lookup even for unchanged handles.
        auto& host = RequireDefenseHost(RequireHostForAnalysis());
        void* const control = host.OwnerControlObjectForAnalysis(GetOwnerForAnalysis());
        const float motion = host.ReadControlMotionForAnalysis(control);
        if (motion > 0.1f)
        {
            host.WriteControlWordForAnalysis(control, 0x3C23D70Au);
            request.packedKey = (request.packedKey & 0xFFFFFFDFu) | 0x50u;
        }
        else
        {
            host.WriteControlWordForAnalysis(control, 0);
            request.packedKey &= 0xFFFFFF8Fu;
        }
        request.packedKey = (request.packedKey & 0xF00001F0u) | 0x180u;
        void* const handle = host.ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        if (field3C_)
            field3C_ = host.IsPendingAnimationCompleteForAnalysis(
                GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true) ? 0 : 1;
        if (field3C_) return;
        ReleasePendingFromState();
        if ((request.packedKey & 0x70u) == 0x50u)
        {
            field3C_ = 1;
            QueuePendingFromState(handle, false, true);
        }
        else QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
}
