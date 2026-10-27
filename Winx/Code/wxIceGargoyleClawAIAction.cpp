#include "wxIceGargoyleClawAIAction.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    using Result=wxIceGargoyleResultForAnalysis;
    namespace
    {
        wxIceGargoyleAIActionHost* factoryHost=nullptr;
        std::unique_ptr<spBaseObject> Create()
        {
            if (!factoryHost) throw std::logic_error("wxIceGargoyleClawAIAction requires stable borrowed host data");
            return std::make_unique<wxIceGargoyleClawAIAction>(*factoryHost);
        }
        const spRTTIRecord record{wxIceGargoyleClawAIAction::ClassID,wxAIAction::ClassID,"wxIceGargoyleClawAIAction",
            &wxAIAction::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxIceGargoyleClawAIAction::wxIceGargoyleClawAIAction(wxIceGargoyleAIActionHost& host,
        wxIceGargoyleUpdateProfileForAnalysis update,wxPerceptionNumericProfileForAnalysis perception)
        :host_(host),updateProfile_(update),perceptionProfile_(perception)
    {
        if (perception!=wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64 &&
            perception!=wxPerceptionNumericProfileForAnalysis::ExactSmallInteger)
            throw std::invalid_argument("IceGargoyle Perception profile is unknown");
    }
    void wxIceGargoyleClawAIAction::SetFactoryHostForAnalysis(wxIceGargoyleAIActionHost* host) noexcept { factoryHost=host; }
    const spRTTIRecord& wxIceGargoyleClawAIAction::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxIceGargoyleClawAIAction::vfunc_18() const noexcept { return record; }
    bool wxIceGargoyleClawAIAction::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        // Native checks only AIAction kind before accessing its physical tail.
        // This portable bounded operation admits a same-kind destination.
        auto* target=dynamic_cast<wxIceGargoyleClawAIAction*>(&destination);
        if (!target || !wxAIAction::vfunc_14(destination,manager)) return false;
        target->state_.CopyFromForAnalysis(state_);
        return true;
    }
    std::unique_ptr<spBaseObject> wxIceGargoyleClawAIAction::vfunc_10(spCloneManager& manager) const
    {
        auto result=std::make_unique<wxIceGargoyleClawAIAction>(host_,updateProfile_,perceptionProfile_);
        manager.RegisterCloneForAnalysis(*this,*result);
        return vfunc_14(*result,manager)?std::move(result):nullptr;
    }
    Result wxIceGargoyleClawAIAction::EnterForAnalysis() noexcept
    { return lastResult_=state_.EnterForAnalysis(*this,host_); }
    void wxIceGargoyleClawAIAction::vfunc_34_ClearForAnalysis() noexcept
    { state_.ExitForAnalysis(*this,host_); }
    Result wxIceGargoyleClawAIAction::QueryForAnalysis() noexcept
    { return lastResult_=state_.QueryForAnalysis(*this,host_,perceptionProfile_); }
    Result wxIceGargoyleClawAIAction::UpdateForAnalysis() noexcept
    {
        if (updateProfile_!=wxIceGargoyleUpdateProfileForAnalysis::PcFiniteGeometry)
            return lastResult_=Result::UnqualifiedUpdateProfile;
        const auto aim=state_.AimForAnalysis(*this,host_);
        if (aim!=Result::Completed) return lastResult_=aim;
        // Native shared Update reads the phase after the complete Aim call.
        if (state_.phase==0) state_.Stage0ForAnalysis(*this,host_);
        else if (state_.phase==1)
            return lastResult_=state_.ClawStage1ForAnalysis(*this,host_);
        return lastResult_=Result::Completed;
    }
    bool wxIceGargoyleClawAIAction::vfunc_24() noexcept
    { return UpdateForAnalysis()==Result::Completed; }
    Result wxIceGargoyleClawAIAction::CheckBoundsForAnalysis() noexcept
    { return lastResult_=state_.ClawBoundsForAnalysis(*this,host_); }
    bool wxIceGargoyleClawAIAction::vfunc_28() noexcept
    {
        const auto result=CheckBoundsForAnalysis();
        return result==Result::Completed || result==Result::SelectorRequested;
    }
}

