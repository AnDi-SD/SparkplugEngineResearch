#include "wxIceWormHolesState.h"
#include "Analysis/Host/wxIceWormHolesStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxIceWormHolesState>(); }
        const spRTTIRecord record{wxIceWormHolesState::ClassID, wxCharacterState::ClassID,
            "wxIceWormHolesState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxIceWormHolesState::wxIceWormHolesState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxIceWormHolesState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxIceWormHolesState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxIceWormHolesState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxIceWormHolesState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxIceWormHolesState::NotifyFlagFromState(const bool flag)
    {
        auto* host = dynamic_cast<wxIceWormHolesStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxIceWormHolesState requires a flag-notification host");
        void* const receiver = host->OwnerField24ForAnalysis(GetOwnerForAnalysis());
        if (receiver) host->SendFlagNotificationForAnalysis(receiver, *this, 0x2739, flag);
    }
    bool wxIceWormHolesState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC5A7990 / PS22ECB30: release occurs BEFORE rewriting request.
        if (GetTransitionFlag1C())
        {
            ReleasePendingFromState();
            request.packedKey = (request.packedKey & 0xFFBFFFDFu) | 0x200050u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            SetPendingHandleFromState(handle);
            QueuePendingFromState(handle, false, true);
            ClearTransitionFlag1C();
            return false;
        }
        if (!RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true)) return false;
        NotifyFlagFromState(false);
        return wxCharacterState::vfunc_1C(request);
    }
    bool wxIceWormHolesState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        // PC5A7A40 / PS22EC9D0. Entry and exit have independent once flags.
        if (GetTransitionFlag1E())
        {
            ReleasePendingFromState();
            request.packedKey = (request.packedKey & 0xFFDFFFDFu) | 0x400050u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            SetPendingHandleFromState(handle);
            QueuePendingFromState(handle, false, true);
            ClearTransitionFlag1E();
            return false;
        }
        if (!RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true)) return false;
        NotifyFlagFromState(true);
        return wxCharacterState::vfunc_20(request);
    }
    void wxIceWormHolesState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        // PC5A7AF0 / PS22EC900. Store pending BEFORE mode-one queue.
        if (!GetTransitionFlag1D()) return;
        ReleasePendingFromState();
        request.packedKey = (request.packedKey & 0xFF9FFFDFu) | 0x50u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        SetPendingHandleFromState(handle);
        QueuePendingFromState(handle, true, true);
        ClearTransitionFlag1D();
    }
}
