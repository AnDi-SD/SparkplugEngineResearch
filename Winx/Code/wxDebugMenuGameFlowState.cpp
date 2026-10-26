#include "wxDebugMenuGameFlowState.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxDebugMenuGameFlowStateHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateState()
        {
            if (!factoryHost) throw std::logic_error("wxDebugMenuGameFlowState requires a factory host");
            return std::make_unique<wxDebugMenuGameFlowState>(*factoryHost);
        }
        const spRTTIRecord record{wxDebugMenuGameFlowState::ClassID,wxGameFlowState::ClassID,
            "wxDebugMenuGameFlowState",&wxGameFlowState::StaticRTTI(),&CreateState,nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        void* Require(void* value)
        {
            if (!value) throw std::logic_error("wxDebugMenuGameFlowState requires borrowed hook services");
            return value; // Portable refusal, not a native fault/exception model.
        }
    }
    wxDebugMenuGameFlowState::wxDebugMenuGameFlowState(wxDebugMenuGameFlowStateHost& host,
        HookPlatformForAnalysis platform) : wxGameFlowState(host),hookHost_(host),platform_(platform) {}
    void wxDebugMenuGameFlowState::SetFactoryHostForAnalysis(wxDebugMenuGameFlowStateHost* host) noexcept
    { factoryHost = host; }
    const spRTTIRecord& wxDebugMenuGameFlowState::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxDebugMenuGameFlowState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxDebugMenuGameFlowState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxDebugMenuGameFlowState>(hookHost_,platform_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager) ? std::move(clone) : nullptr;
    }
    void wxDebugMenuGameFlowState::FlowSlot20ForAnalysis()
    {
        auto* owner=Require(hookHost_.ResolveHookInputOwnerForAnalysis());
        hookHost_.ResetHookInputForAnalysis(Require(hookHost_.ReadHookInputForAnalysis(owner)));
        // Both native versions capture the owner before Reset and reload its
        // member1C afterwards. A changed global owner does not replace it.
        hookHost_.SetHookInputModeForAnalysis(Require(hookHost_.ReadHookInputForAnalysis(owner)),7);
        if (platform_==HookPlatformForAnalysis::PC)
            hookHost_.InvokeDebugSlot20ConsumerForAnalysis(Require(hookHost_.ResolveDebugServiceForAnalysis()));
    }
    void wxDebugMenuGameFlowState::FlowSlot24ForAnalysis()
    {
        auto* owner=Require(hookHost_.ResolveHookInputOwnerForAnalysis());
        hookHost_.ResetHookInputForAnalysis(Require(hookHost_.ReadHookInputForAnalysis(owner)));
        if (platform_==HookPlatformForAnalysis::PC)
            hookHost_.InvokeDebugSlot24ConsumerForAnalysis(Require(hookHost_.ResolveDebugServiceForAnalysis()));
    }
}
