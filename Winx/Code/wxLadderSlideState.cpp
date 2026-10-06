#include "wxLadderSlideState.h"
#include "Analysis/Host/wxLadderSlideStateHost.h"
#include <stdexcept>
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxLadderSlideState>(); }
        const spRTTIRecord Record{wxLadderSlideState::ClassID, wxCharacterState::ClassID,
            "wxLadderSlideState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    wxLadderSlideState::wxLadderSlideState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxLadderSlideState::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& wxLadderSlideState::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> wxLadderSlideState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxLadderSlideState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxLadderSlideState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        if (GetTransitionFlag1C())
        {
            request.packedKey = (request.packedKey & 0xf0a0002fu) | 0xa00020u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            ClearTransitionFlag1C();
        }
        else if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
            return wxCharacterState::vfunc_1C(request);
        ClearOwnerActionControlFromState();
        return false;
    }
    bool wxLadderSlideState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        auto* host = dynamic_cast<wxLadderSlideStateHost*>(&RequireHostForAnalysis());
        if (!host) throw std::logic_error("wxLadderSlideState requires an owner exit-flags host");
        if (!(host->OwnerLadderExitFlagsForAnalysis(GetOwnerForAnalysis()) & 0xfu))
            return wxCharacterState::vfunc_20(request);
        if (GetTransitionFlag1E())
        {
            ReleasePendingFromState();
            // Read the caller's current key after release callbacks.
            request.packedKey = (request.packedKey & 0xf0c0002fu) | 0xc00020u;
            void* const handle = host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            ClearTransitionFlag1E();
        }
        else if (host->IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
            return wxCharacterState::vfunc_20(request);
        ClearOwnerActionControlFromState();
        return false;
    }
    void wxLadderSlideState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey = (request.packedKey & 0xf080002fu) | 0x800020u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
    bool wxLadderSlideState::vfunc_34(std::uint32_t code)
    { return code == 0 || code == 10 || code == 14; }
    std::uint32_t wxLadderSlideState::vfunc_38(std::uint32_t) const { return 1; }
}
