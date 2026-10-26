#include "wxIdleAIAction.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxIdleAIActionHost* factoryHost = nullptr;
        auto factoryProfile = wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64;
        std::unique_ptr<spBaseObject> CreateIdleAction()
        {
            if (factoryHost == nullptr) throw std::logic_error("wxIdleAIAction requires a factory host");
            return std::make_unique<wxIdleAIAction>(*factoryHost, factoryProfile);
        }
        const spRTTIRecord record{wxIdleAIAction::ClassID, wxAIAction::ClassID,
            "wxIdleAIAction", &wxAIAction::StaticRTTI(), &CreateIdleAction, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxIdleAIAction::wxIdleAIAction(wxIdleAIActionHost& host,
        const wxPerceptionNumericProfileForAnalysis profile) noexcept
        : host_(host), numericProfile_(profile) {}
    void wxIdleAIAction::SetFactoryHostForAnalysis(wxIdleAIActionHost* host,
        const wxPerceptionNumericProfileForAnalysis profile) noexcept
    { factoryHost = host; factoryProfile = profile; }
    const spRTTIRecord& wxIdleAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxIdleAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxIdleAIAction::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxIdleAIAction>(host_, numericProfile_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool wxIdleAIAction::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        if (dynamic_cast<wxIdleAIAction*>(&destination) == nullptr) return false;
        // PC5D67B0 / PS225AE70 transfer no own or inherited payload after the
        // base destination cleanup. The class-tag completion is an actual no-op.
        return wxAIAction::vfunc_14(destination, manager);
    }
    void wxIdleAIAction::vfunc_0C(const void*) noexcept {} // PC5B7A00 / PS225AEE0
    void wxIdleAIAction::vfunc_34_ClearForAnalysis() noexcept {} // PC48EAA0 / PS225AF00
    void wxIdleAIAction::vfunc_30() noexcept
    {
        // Native entry has an unused argument. A null command suppresses only
        // its reset; the action's virtual query still executes.
        auto* command = host_.ReadCommandForAnalysis(GetOwnerForAnalysis());
        if (command != nullptr) host_.ResetCommandForAnalysis(*this, command);
        vfunc_38();
    }
    void wxIdleAIAction::vfunc_38() noexcept
    {
        auto* capturedOwner = GetOwnerForAnalysis();
        if (host_.ReadPermissionForAnalysis(capturedOwner) == 0) return;
        auto& perception = host_.ReadPerceptionForAnalysis(capturedOwner);
        // Required host positions must satisfy the selected numeric domain.
        // The native methods have no exception boundary either.
        if (perception.FindNearestForAnalysis(numericProfile_) != nullptr)
            host_.SelectActionForAnalysis(GetOwnerForAnalysis(), 2, 0);
        // Re-read the action owner after the query, as both originals do.
    }
}
