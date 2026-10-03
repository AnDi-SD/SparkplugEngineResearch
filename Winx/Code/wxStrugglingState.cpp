#include "wxStrugglingState.h"
#include "Analysis/Host/wxStrugglingStateHost.h"
#include <cstring>
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxStrugglingState>(); }
        const spRTTIRecord record{wxStrugglingState::ClassID, wxCharacterState::ClassID,
            "wxStrugglingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxStrugglingState::wxStrugglingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxStrugglingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxStrugglingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxStrugglingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxStrugglingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxStrugglingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC524090 / PS22D42B0: virtual update, without base entry release.
        vfunc_30(request);
        return true;
    }
    bool wxStrugglingState::vfunc_20(wxAnimationRequestForAnalysis&)
    {
        // PC51F8B0 / PS22D4170.
        ReleasePendingFromState();
        return true;
    }
    void wxStrugglingState::vfunc_30(wxAnimationRequestForAnalysis&)
    {
        // PC51F8E0 / PS22D4190: caller's request is untouched.
        auto* host = dynamic_cast<wxStrugglingStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxStrugglingState requires an entity-controller host");
        ClearOwnerActionControlFromState();
        host->ResetOwnerEntityControllerForAnalysis(GetOwnerForAnalysis());
        void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), 0x20000);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
}
