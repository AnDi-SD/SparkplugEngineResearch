#include "wxIceBatFlyingState.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxIceBatFlyingState>(); }
        const spRTTIRecord record{wxIceBatFlyingState::ClassID, wxCharacterState::ClassID,
            "wxIceBatFlyingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxIceBatFlyingState::wxIceBatFlyingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxIceBatFlyingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxIceBatFlyingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxIceBatFlyingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxIceBatFlyingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxIceBatFlyingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        if (!GetTransitionFlag1D()) return;
        ReleasePendingFromState();
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        SetPendingHandleFromState(handle);
        QueuePendingFromState(handle, true, true);
        ClearTransitionFlag1D();
    }
}
