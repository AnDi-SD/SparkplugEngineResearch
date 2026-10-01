#include "wxShadowBeastJumpingState.h"

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateShadowBeastJumpingState()
        { return std::make_unique<wxShadowBeastJumpingState>(); }
        const spRTTIRecord record{wxShadowBeastJumpingState::ClassID, wxCharacterState::ClassID,
            "wxShadowBeastJumpingState", &wxCharacterState::StaticRTTI(), &CreateShadowBeastJumpingState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxShadowBeastJumpingState::wxShadowBeastJumpingState() noexcept
    { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxShadowBeastJumpingState::StaticRTTI() noexcept
    { (void)registered; return record; }
    const spRTTIRecord& wxShadowBeastJumpingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxShadowBeastJumpingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxShadowBeastJumpingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxShadowBeastJumpingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        // PC523805 / PS230FFBC: release precedes even the key rewrite.
        ReleasePendingFromState();
        request.packedKey = (request.packedKey & 0xF0087FA2u) | 0x00080022u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(
            GetOwnerForAnalysis(), request.packedKey);
        QueuePendingFromState(handle, false, true);
        SetPendingHandleFromState(handle);
        ClearTransitionFlag1C();
        vfunc_30(request);
        return true;
    }
    bool wxShadowBeastJumpingState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey = (request.packedKey & 0xFFFFFFF2u) | 2u;
        ClearOwnerActionControlFromState();
        request.packedKey &= 0xFFFFFFF0u;
        return wxCharacterState::vfunc_20(request);
    }
    void wxShadowBeastJumpingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    { request.packedKey = (request.packedKey & 0xFFFFFFF2u) | 2u; }
    bool wxShadowBeastJumpingState::vfunc_34(std::uint32_t)
    {
        // Unlike Dispel/WayToGo, the native body queries even a null handle.
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
}
