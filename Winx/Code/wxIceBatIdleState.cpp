#include "wxIceBatIdleState.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxIceBatIdleState>(); }
        const spRTTIRecord record{wxIceBatIdleState::ClassID, wxCharacterState::ClassID,
            "wxIceBatIdleState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxIceBatIdleState::wxIceBatIdleState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxIceBatIdleState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxIceBatIdleState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxIceBatIdleState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxIceBatIdleState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxIceBatIdleState::vfunc_20(wxAnimationRequestForAnalysis& request)
    {
        if (GetTransitionFlag1E())
        {
            // PC519A9E: release observes the caller's unmodified key.
            ReleasePendingFromState();
            request.packedKey = (request.packedKey & 0xFFDFFFFFu) | 0x00400000u;
            void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
            SetPendingHandleFromState(handle);
            QueuePendingFromState(handle, false, true);
            ClearTransitionFlag1E();
            return false;
        }
        if (!RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true)) return false;
        return wxCharacterState::vfunc_20(request);
    }
    void wxIceBatIdleState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        if (!GetTransitionFlag1D()) return;
        ReleasePendingFromState();
        request.packedKey &= 0xFF9FFFFFu;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        SetPendingHandleFromState(handle);
        QueuePendingFromState(handle, true, true);
        ClearTransitionFlag1D();
    }
}
