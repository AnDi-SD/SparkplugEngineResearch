#include "wxSpiritFollowState.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxSpiritFollowState>(); }
        const spRTTIRecord record{wxSpiritFollowState::ClassID, wxCharacterState::ClassID,
            "wxSpiritFollowState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxSpiritFollowState::wxSpiritFollowState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxSpiritFollowState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxSpiritFollowState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxSpiritFollowState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxSpiritFollowState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxSpiritFollowState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        if (!GetTransitionFlag1D()) return;
        ReleasePendingFromState();
        request.packedKey = (request.packedKey & 0xFF9FFFDFu) | 0x50u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        SetPendingHandleFromState(handle);
        QueuePendingFromState(handle, true, true);
        ClearTransitionFlag1D();
    }
}
