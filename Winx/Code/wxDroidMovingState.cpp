#include "wxDroidMovingState.h"
#include "Analysis/Host/wxCharacterMotionStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxDroidMovingState>(); }
        const spRTTIRecord record{wxDroidMovingState::ClassID, wxCharacterState::ClassID,
            "wxDroidMovingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxDroidMovingState::wxDroidMovingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxDroidMovingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxDroidMovingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxDroidMovingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxDroidMovingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxDroidMovingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        auto* host = dynamic_cast<wxCharacterMotionStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxDroidMovingState requires a motion host");
        // PC523FD0 / PS2312990: equality belongs to the stationary branch.
        if (host->OwnerMotionForAnalysis(GetOwnerForAnalysis()) <= 0.1f)
            request.packedKey &= 0xFFFFFF8Fu;
        else
            request.packedKey = (request.packedKey & 0xFFFFFFDFu) | 0x50u;
        request.packedKey &= 0xF007FFFFu;
        void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
}
