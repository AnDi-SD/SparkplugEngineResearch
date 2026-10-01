#include "wxDateTalkingState.h"
#include "wxDateStateOperationsForAnalysis.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxDateTalkingState>(); }
        const spRTTIRecord record{wxDateTalkingState::ClassID, wxCharacterState::ClassID,
            "wxDateTalkingState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxDateTalkingState::wxDateTalkingState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxDateTalkingState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxDateTalkingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxDateTalkingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxDateTalkingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxDateTalkingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    { vfunc_30(request); return true; }
    bool wxDateTalkingState::vfunc_20(wxAnimationRequestForAnalysis& request)
    { return ExitDateStateForAnalysis(*this, request, GetOwnerForAnalysis(), RequireHostForAnalysis()); }
    void wxDateTalkingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey = (request.packedKey & 0xFF800E09u) | 0xE09u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        if (handle == GetPendingHandleForAnalysis()) return;
        ReleasePendingFromState();
        QueuePendingFromState(handle, true, true);
        SetPendingHandleFromState(handle);
    }
}
