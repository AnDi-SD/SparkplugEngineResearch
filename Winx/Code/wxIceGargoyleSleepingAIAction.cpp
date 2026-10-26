#include "wxIceGargoyleSleepingAIAction.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxIceGargoyleSleepingAIActionHost* factoryHost = nullptr;
        auto factoryProfile = wxSleepingTargetProfileForAnalysis::ExistingOnly;
        std::unique_ptr<spBaseObject> CreateSleepingAction()
        {
            if (!factoryHost) throw std::logic_error("Sleeping action requires a factory host");
            return std::make_unique<wxIceGargoyleSleepingAIAction>(*factoryHost, factoryProfile);
        }
        const spRTTIRecord record{wxIceGargoyleSleepingAIAction::ClassID, wxAIAction::ClassID,
            "wxIceGargoyleSleepingAIAction", &wxAIAction::StaticRTTI(), &CreateSleepingAction, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxIceGargoyleSleepingAIAction::wxIceGargoyleSleepingAIAction(
        wxIceGargoyleSleepingAIActionHost& host, wxSleepingTargetProfileForAnalysis profile) noexcept
        : host_(host), targetProfile_(profile) {}
    void wxIceGargoyleSleepingAIAction::SetFactoryHostForAnalysis(
        wxIceGargoyleSleepingAIActionHost* host, wxSleepingTargetProfileForAnalysis profile) noexcept
    { factoryHost = host; factoryProfile = profile; }
    const spRTTIRecord& wxIceGargoyleSleepingAIAction::StaticRTTI() noexcept
    { (void)registered; return record; }
    const spRTTIRecord& wxIceGargoyleSleepingAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxIceGargoyleSleepingAIAction::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxIceGargoyleSleepingAIAction>(host_, targetProfile_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool wxIceGargoyleSleepingAIAction::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxIceGargoyleSleepingAIAction*>(&destination);
        if (!target || !wxAIAction::vfunc_14(destination, manager)) return false;
        target->phase_ = phase_; target->stamp_ = stamp_;
        return true;
    }
    void wxIceGargoyleSleepingAIAction::vfunc_30() noexcept { phase_ = 0; stamp_ = 0; }
    void wxIceGargoyleSleepingAIAction::vfunc_34_ClearForAnalysis() noexcept
    { host_.WriteCommandByteForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()), 0x1B, 0); }
    void wxIceGargoyleSleepingAIAction::vfunc_38() noexcept {} // Native own query is a no-op.
    void wxIceGargoyleSleepingAIAction::WakeForAnalysis() noexcept
    {
        if (phase_ != 1) { phase_ = 1; stamp_ = 0; }
    }
    void wxIceGargoyleSleepingAIAction::vfunc_0C(const void* notification) noexcept
    {
        if (static_cast<const wxAIActionMessageForAnalysis*>(notification)->code == 0x27B2)
            WakeForAnalysis();
        else wxAIAction::vfunc_0C(notification);
    }
    void wxIceGargoyleSleepingAIAction::Stage0ForAnalysis() noexcept
    {
        if (stamp_ != 0) return;
        host_.WriteCommandByteForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()), 0x1B, 1);
        stamp_ = host_.GameTimerPrefixForAnalysis().GetStateForAnalysis().currentMilliseconds;
    }
    wxSleepingTickResultForAnalysis wxIceGargoyleSleepingAIAction::Stage1ForAnalysis() noexcept
    {
        if (stamp_ == 0)
        {
            host_.WriteCommandByteForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()), 0x1B, 0);
            stamp_ = host_.GameTimerPrefixForAnalysis().GetStateForAnalysis().currentMilliseconds + std::uint32_t{100};
            if (targetProfile_ == wxSleepingTargetProfileForAnalysis::BoundedPcAcquisition)
            {
                const auto binding = host_.TargetManagerBindingForAnalysis();
                if (binding.platform != wxSleepingTargetPlatformForAnalysis::PC)
                    return wxSleepingTickResultForAnalysis::UnqualifiedTargetManagerAcquisition;
                if (wxTargetManager::GetInstanceForAnalysis()) return wxSleepingTickResultForAnalysis::Completed;
                if (!binding.services || !binding.applicationOwners)
                    return wxSleepingTickResultForAnalysis::UnqualifiedTargetManagerAcquisition;
                try
                {
                    auto created = std::make_unique<wxTargetManager>(*binding.services);
                    // Native caller5AF20D stores the factory's returned object
                    // again after its constructor and before returning.
                    wxTargetManager::PublishInstanceForAnalysis(*created);
                    binding.applicationOwners->push_back(std::move(created));
                }
                catch (...)
                {
                    // Outside the admitted successful resource domain. This
                    // analytical refusal makes no native failure/rollback claim.
                    return wxSleepingTickResultForAnalysis::UnqualifiedTargetManagerAcquisition;
                }
                return wxSleepingTickResultForAnalysis::Completed;
            }
            if (!host_.HasExistingTargetManagerForAnalysis())
                return wxSleepingTickResultForAnalysis::UnqualifiedTargetManagerAcquisition;
            return wxSleepingTickResultForAnalysis::Completed;
        }
        if (host_.ReadCharacterStateWordForAnalysis(GetOwnerForAnalysis()) == 0)
        {
            host_.SelectActionForAnalysis(GetOwnerForAnalysis(), 0, 0);
            return wxSleepingTickResultForAnalysis::SelectorBoundary;
        }
        return wxSleepingTickResultForAnalysis::Completed;
    }
    wxSleepingTickResultForAnalysis wxIceGargoyleSleepingAIAction::TickForAnalysis() noexcept
    {
        if (phase_ == 0) Stage0ForAnalysis();
        else if (phase_ == 1) return Stage1ForAnalysis();
        return wxSleepingTickResultForAnalysis::Completed;
    }
    bool wxIceGargoyleSleepingAIAction::vfunc_24() noexcept
    {
        // Native ordinary returns are true. False here is our explicit cold
        // acquisition guard; integrations needing the distinction use Tick.
        return TickForAnalysis() != wxSleepingTickResultForAnalysis::UnqualifiedTargetManagerAcquisition;
    }
}
