#include "wxStickingRightState.h"
#include "wxStickingStateOperationsForAnalysis.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxStickingRightState>(); }
        const spRTTIRecord record{wxStickingRightState::ClassID, wxCharacterState::ClassID,
            "wxStickingRightState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxStickingRightState::wxStickingRightState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxStickingRightState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxStickingRightState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxStickingRightState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxStickingRightState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxStickingRightState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    { return wxStickingStateOperationsForAnalysis::Enter<true>(*this, request); }
    bool wxStickingRightState::vfunc_20(wxAnimationRequestForAnalysis& request)
    { return wxStickingStateOperationsForAnalysis::Exit<true>(*this, request); }
    void wxStickingRightState::vfunc_30(wxAnimationRequestForAnalysis& request)
    { wxStickingStateOperationsForAnalysis::Update<true>(*this, request); }
}
