#include "wxMikaelOpenGateState.h"
namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxMikaelOpenGateState>(); }
        const spRTTIRecord record{wxMikaelOpenGateState::ClassID, wxCharacterState::ClassID,
            "wxMikaelOpenGateState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxMikaelOpenGateState::wxMikaelOpenGateState() noexcept
    { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxMikaelOpenGateState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxMikaelOpenGateState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxMikaelOpenGateState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxMikaelOpenGateState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxMikaelOpenGateState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    { return wxNPCStateOperationsForAnalysis::Enter(*this, request); }
    void wxMikaelOpenGateState::vfunc_30(wxAnimationRequestForAnalysis& request)
    { wxNPCStateOperationsForAnalysis::UpdateModeZero(*this, request, 0xF1000000u, 0x1000000u); }
}
