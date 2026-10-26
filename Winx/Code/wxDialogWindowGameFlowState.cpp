#include "wxDialogWindowGameFlowState.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxDialogWindowGameFlowStateHost* factoryHost=nullptr;
        std::unique_ptr<spBaseObject> CreateState()
        {
            if (!factoryHost) throw std::logic_error("wxDialogWindowGameFlowState requires a factory host");
            return std::make_unique<wxDialogWindowGameFlowState>(*factoryHost);
        }
        const spRTTIRecord record{wxDialogWindowGameFlowState::ClassID,wxGameFlowState::ClassID,
            "wxDialogWindowGameFlowState",&wxGameFlowState::StaticRTTI(),&CreateState,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        void* Require(void* value)
        {
            if (!value) throw std::logic_error("wxDialogWindowGameFlowState requires a borrowed window");
            return value; // Portable guard, not native lazy-construction/fault behavior.
        }
    }
    wxDialogWindowGameFlowState::wxDialogWindowGameFlowState(wxDialogWindowGameFlowStateHost& host)
        : wxGameFlowState(host),hookHost_(host) {}
    void wxDialogWindowGameFlowState::SetFactoryHostForAnalysis(wxDialogWindowGameFlowStateHost* host) noexcept
    { factoryHost=host; }
    const spRTTIRecord& wxDialogWindowGameFlowState::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxDialogWindowGameFlowState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxDialogWindowGameFlowState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxDialogWindowGameFlowState>(hookHost_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager) ? std::move(clone) : nullptr;
    }
    void wxDialogWindowGameFlowState::FlowSlot24ForAnalysis()
    {
        auto* window=Require(hookHost_.ResolveDialogWindowForAnalysis());
        // PC REP STOSD count150 and PS2 memset length258 both clear exactly
        // window[14,26C). The host supplies storage; the class performs the fill.
        hookHost_.BorrowDialogWindowResetRegionForAnalysis(window).fill(0);
    }
    bool wxDialogWindowGameFlowState::FlowSlot30ForAnalysis()
    {
        auto* window=Require(hookHost_.ResolveDialogWindowForAnalysis());
        hookHost_.InvokeDialogWindowConsumerForAnalysis(window);
        return true; // Literal true after the consumer in both original builds.
    }
}
