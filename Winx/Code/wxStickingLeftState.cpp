#include "wxStickingLeftState.h"
#include "wxStickingStateOperationsForAnalysis.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateState() { return std::make_unique<wxStickingLeftState>(); }
        const spRTTIRecord record{wxStickingLeftState::ClassID, wxCharacterState::ClassID,
            "wxStickingLeftState", &wxCharacterState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxStickingLeftState::wxStickingLeftState() noexcept { SetStateSelectorForConstruction(StateSelector); }
    const spRTTIRecord& wxStickingLeftState::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxStickingLeftState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxStickingLeftState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxStickingLeftState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxStickingLeftState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    { return wxStickingStateOperationsForAnalysis::Enter<false>(*this, request); }
    bool wxStickingLeftState::vfunc_20(wxAnimationRequestForAnalysis& request)
    { return wxStickingStateOperationsForAnalysis::Exit<false>(*this, request); }
    void wxStickingLeftState::vfunc_30(wxAnimationRequestForAnalysis& request)
    { wxStickingStateOperationsForAnalysis::Update<false>(*this, request); }
}
