#include "wxFlyingState.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxFlyingState>(); }
        const spRTTIRecord record{wxFlyingState::ClassID, wxCharacterState::ClassID,
            "wxFlyingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxFlyingState::wxFlyingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxFlyingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxFlyingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxFlyingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxFlyingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxFlyingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey = (request.packedKey & 0xF007FFF1u) | 1u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
}
