#include "wxFrogBackFlipState.h"
#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxFrogBackFlipState>(); }
        const spRTTIRecord record{wxFrogBackFlipState::ClassID, wxCharacterState::ClassID,
            "wxFrogBackFlipState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxFrogBackFlipState::wxFrogBackFlipState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxFrogBackFlipState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxFrogBackFlipState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxFrogBackFlipState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxFrogBackFlipState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxFrogBackFlipState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC520FF0 / PS22F3FF0: no old-pending release or virtual update.
        PrepareMovementFromState(false);
        request.packedKey = (request.packedKey & 0xF0080850u) | 0x80850u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        return true;
    }
    void wxFrogBackFlipState::vfunc_30(wxAnimationRequestForAnalysis&)
    {
        // PC520FB0 / PS22F3FB0: control clear occurs after movement finish.
        FinishMovementFromState(false);
        ClearOwnerActionControlFromState();
    }
    bool wxFrogBackFlipState::vfunc_34(std::uint32_t)
    {
        // PC523770 / PS22F4150: query also runs when pending is null.
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
}
