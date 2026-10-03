#include "wxWandringNPCWaitState.h"
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxWandringNPCWaitState>(); }
        const spRTTIRecord record{wxWandringNPCWaitState::ClassID, wxCharacterState::ClassID,
            "wxWandringNPCWaitState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxWandringNPCWaitState::wxWandringNPCWaitState() noexcept
    { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxWandringNPCWaitState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxWandringNPCWaitState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxWandringNPCWaitState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxWandringNPCWaitState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxWandringNPCWaitState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    { return wxNPCStateOperationsForAnalysis::Enter(*this, request); }
    void wxWandringNPCWaitState::vfunc_30(wxAnimationRequestForAnalysis& request)
    { wxNPCStateOperationsForAnalysis::UpdateModeZero(*this, request, 0xF0000000u, 0u); }
}
