#include "wxLoadSaveGameFlowState.h"

#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxLoadSaveGameFlowStateHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateState()
        {
            if (!factoryHost) throw std::logic_error("wxLoadSaveGameFlowState requires a factory host");
            return std::make_unique<wxLoadSaveGameFlowState>(*factoryHost);
        }
        const spRTTIRecord record{wxLoadSaveGameFlowState::ClassID, wxGameFlowState::ClassID,
            "wxLoadSaveGameFlowState", &wxGameFlowState::StaticRTTI(), &CreateState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);

        void* Require(void* token)
        {
            if (!token) throw std::logic_error("wxLoadSaveGameFlowState requires borrowed hook services");
            return token; // Portable guard; not native null/fault behavior.
        }
    }

    wxLoadSaveGameFlowState::wxLoadSaveGameFlowState(wxLoadSaveGameFlowStateHost& host,
        const HookPlatformForAnalysis platform)
        : wxGameFlowState(host), hookHost_(host), platform_(platform)
    {
    }

    void wxLoadSaveGameFlowState::SetFactoryHostForAnalysis(wxLoadSaveGameFlowStateHost* host) noexcept
    {
        factoryHost = host;
    }

    const spRTTIRecord& wxLoadSaveGameFlowState::StaticRTTI() noexcept
    {
        (void)registered;
        return record;
    }

    const spRTTIRecord& wxLoadSaveGameFlowState::vfunc_18() const noexcept
    {
        return record;
    }

    std::unique_ptr<spBaseObject> wxLoadSaveGameFlowState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxLoadSaveGameFlowState>(hookHost_, platform_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    void wxLoadSaveGameFlowState::FlowSlot24ForAnalysis()
    {
        auto* owner = Require(hookHost_.ResolveHookInputOwnerForAnalysis());
        hookHost_.ResetHookInputForAnalysis(Require(hookHost_.ReadHookInputForAnalysis(owner)));
        if (platform_ == HookPlatformForAnalysis::PS2)
        {
            auto* manager = Require(hookHost_.ResolveLoadSaveManagerForAnalysis());
            if (hookHost_.ReadLoadSaveStateForAnalysis(manager) == 4)
            {
                auto* controller = Require(hookHost_.ResolveLoadSaveControllerForAnalysis());
                hookHost_.InvokeLoadSaveControllerForAnalysis(controller, 0x36, 1);
            }
        }
    }

    bool wxLoadSaveGameFlowState::FlowSlot30ForAnalysis()
    {
        auto* owner = Require(hookHost_.ResolveHookInputOwnerForAnalysis());
        hookHost_.ResetHookInputForAnalysis(Require(hookHost_.ReadHookInputForAnalysis(owner)));
        // The original retains the owner/member address across Reset, then
        // reloads member1C. Reset may replace the input behind that member.
        hookHost_.SetHookInputModeForAnalysis(Require(hookHost_.ReadHookInputForAnalysis(owner)), 7);
        if (platform_ == HookPlatformForAnalysis::PS2)
        {
            auto* manager = Require(hookHost_.ResolveLoadSaveManagerForAnalysis());
            hookHost_.UpdateLoadSaveManagerForAnalysis(manager);
        }
        return true; // PC literal AL=1; PS2 literal V0=1 after the external call.
    }
}
