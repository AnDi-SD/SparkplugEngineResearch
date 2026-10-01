#include "wxDateIdleState.h"
#include "wxDateStateOperationsForAnalysis.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxDateIdleState>(); }
        const spRTTIRecord record{wxDateIdleState::ClassID, wxCharacterState::ClassID,
            "wxDateIdleState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxDateIdleState::wxDateIdleState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxDateIdleState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxDateIdleState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxDateIdleState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxDateIdleState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxDateIdleState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    { vfunc_30(request); return true; }
    bool wxDateIdleState::vfunc_20(wxAnimationRequestForAnalysis& request)
    { return ExitDateStateForAnalysis(*this, request, GetOwnerForAnalysis(), RequireHostForAnalysis()); }
    void wxDateIdleState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey = (request.packedKey & 0xFF800059u) | 0x59u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
}
