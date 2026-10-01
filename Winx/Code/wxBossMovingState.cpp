#include "wxBossMovingState.h"
#include "Analysis/Host/wxBossMovingStateHost.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxBossMovingState>(); }
        const spRTTIRecord record{wxBossMovingState::ClassID, wxCharacterState::ClassID,
            "wxBossMovingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxBossMovingState::wxBossMovingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxBossMovingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxBossMovingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxBossMovingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxBossMovingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxBossMovingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        auto* host = dynamic_cast<wxBossMovingStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxBossMovingState requires a boss-moving host");
        if (host->ReadOwnerMotionWordForAnalysis(GetOwnerForAnalysis(), 4) == 0.0f)
            request.packedKey &= 0xFFFFFF8Fu;
        else if (host->ReadOwnerMotionWordForAnalysis(GetOwnerForAnalysis(), 8) > 0.0f)
            request.packedKey = (request.packedKey & 0xFFFFFFCFu) | 0x40u;
        else
            request.packedKey = (request.packedKey & 0xFFFFFFBFu) | 0x30u;
        request.packedKey &= 0xF01FFFFFu;
        void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
        // PC floating-point branches are preserved. PS2 is qualified for
        // normal finite inputs and zeros; special/denormal EE FPU values
        // require separate qualification.
    }
}
