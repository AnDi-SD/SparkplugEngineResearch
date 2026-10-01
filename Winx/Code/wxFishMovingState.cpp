#include "wxFishMovingState.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxFishMovingState>(); }
        const spRTTIRecord record{wxFishMovingState::ClassID, wxCharacterState::ClassID,
            "wxFishMovingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxFishMovingState::wxFishMovingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxFishMovingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxFishMovingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxFishMovingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxFishMovingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    void wxFishMovingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        if (!GetTransitionFlag1D()) return;
        request.packedKey = (request.packedKey & 0xFF800050u) | 0x50u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        const bool mode = (request.packedKey & 0x0F800000u) == 0;
        QueuePendingFromState(handle, mode, true);
        SetPendingHandleFromState(handle);
        // Neither release nor clearing flag1D occurs in this native body.
    }
    bool wxFishMovingState::vfunc_34(std::uint32_t)
    {
        if (GetStateSelectorForAnalysis() == 0 || GetPendingHandleForAnalysis() == nullptr) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
}
