#include "wxDroidInactiveState.h"

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDroidInactiveState()
        { return std::make_unique<wxDroidInactiveState>(); }
        const spRTTIRecord record{wxDroidInactiveState::ClassID, wxCharacterState::ClassID,
            "wxDroidInactiveState", &wxCharacterState::StaticRTTI(), &CreateDroidInactiveState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxDroidInactiveState::wxDroidInactiveState() noexcept
    { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxDroidInactiveState::StaticRTTI() noexcept
    { (void)registered; return record; }
    const spRTTIRecord& wxDroidInactiveState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxDroidInactiveState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxDroidInactiveState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxDroidInactiveState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        if (GetTransitionFlag1E())
        {
            request.packedKey = (request.packedKey & 0xF0478003u) | 0x00400003u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(
                GetOwnerForAnalysis(), request.packedKey);
            ReleasePendingFromState();
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
            ClearTransitionFlag1E();
        }
        else if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true))
        {
            return wxCharacterState::vfunc_20(request);
        }
        ClearOwnerActionControlFromState();
        return false;
    }
    void wxDroidInactiveState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey = (request.packedKey & 0xF0078003u) | 3u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(
            GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
}
