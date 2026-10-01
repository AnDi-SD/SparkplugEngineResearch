#include "wxDateReactionState.h"
#include "wxDateStateOperationsForAnalysis.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxDateReactionState>(); }
        const spRTTIRecord record{wxDateReactionState::ClassID, wxCharacterState::ClassID,
            "wxDateReactionState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxDateReactionState::wxDateReactionState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxDateReactionState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxDateReactionState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxDateReactionState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxDateReactionState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxDateReactionState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey = (request.packedKey & 0xFF800009u) | 9u;
        void* const handle = RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(), request.packedKey);
        // A null lookup leaves the original pending handle untouched.
        if (handle != nullptr)
        {
            ReleasePendingFromState();
            QueuePendingFromState(handle, false, true);
            SetPendingHandleFromState(handle);
        }
        return true;
    }
    bool wxDateReactionState::vfunc_20(wxAnimationRequestForAnalysis& request)
    { return ExitDateStateForAnalysis(*this, request, GetOwnerForAnalysis(), RequireHostForAnalysis()); }
    bool wxDateReactionState::vfunc_34(std::uint32_t)
    {
        if (GetPendingHandleForAnalysis() == nullptr) return true;
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(), GetPendingHandleForAnalysis(), true);
    }
}
