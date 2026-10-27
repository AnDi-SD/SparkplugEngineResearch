#include "wxIceGargoyleWithdrawlAIAction.h"
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
            if (!factoryHost) throw std::logic_error("wxIceGargoyleWithdrawlAIAction requires stable borrowed host data");
            return std::make_unique<wxIceGargoyleWithdrawlAIAction>(*factoryHost);
        }
        const spRTTIRecord record{wxIceGargoyleWithdrawlAIAction::ClassID,wxAIAction::ClassID,"wxIceGargoyleWithdrawlAIAction",
            &wxAIAction::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxIceGargoyleWithdrawlAIAction::wxIceGargoyleWithdrawlAIAction(wxIceGargoyleAIActionHost& host,
        wxIceGargoyleUpdateProfileForAnalysis update,wxPerceptionNumericProfileForAnalysis perception)
        :host_(host),updateProfile_(update),perceptionProfile_(perception)
    {
        if (perception!=wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64 &&
            perception!=wxPerceptionNumericProfileForAnalysis::ExactSmallInteger)
            throw std::invalid_argument("IceGargoyle Perception profile is unknown");
    }
    void wxIceGargoyleWithdrawlAIAction::SetFactoryHostForAnalysis(wxIceGargoyleAIActionHost* host) noexcept { factoryHost=host; }
    const spRTTIRecord& wxIceGargoyleWithdrawlAIAction::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxIceGargoyleWithdrawlAIAction::vfunc_18() const noexcept { return record; }
    bool wxIceGargoyleWithdrawlAIAction::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        // Native checks only AIAction kind before accessing its physical tail.
        // This portable bounded operation admits a same-kind destination.
        auto* target=dynamic_cast<wxIceGargoyleWithdrawlAIAction*>(&destination);
        if (!target || !wxAIAction::vfunc_14(destination,manager)) return false;
        target->state_.CopyFromForAnalysis(state_);
        return true;
    }
    std::unique_ptr<spBaseObject> wxIceGargoyleWithdrawlAIAction::vfunc_10(spCloneManager& manager) const
    {
        auto result=std::make_unique<wxIceGargoyleWithdrawlAIAction>(host_,updateProfile_,perceptionProfile_);
        manager.RegisterCloneForAnalysis(*this,*result);
        return vfunc_14(*result,manager)?std::move(result):nullptr;
    }
    Result wxIceGargoyleWithdrawlAIAction::EnterForAnalysis() noexcept
    { return lastResult_=state_.EnterForAnalysis(*this,host_); }
    void wxIceGargoyleWithdrawlAIAction::vfunc_34_ClearForAnalysis() noexcept
    { state_.ExitForAnalysis(*this,host_); }
    Result wxIceGargoyleWithdrawlAIAction::QueryForAnalysis() noexcept
    { return lastResult_=state_.QueryForAnalysis(*this,host_,perceptionProfile_); }
    Result wxIceGargoyleWithdrawlAIAction::UpdateForAnalysis() noexcept
    {
        if (updateProfile_!=wxIceGargoyleUpdateProfileForAnalysis::PcFiniteGeometry)
            return lastResult_=Result::UnqualifiedUpdateProfile;
        const auto aim=state_.AimForAnalysis(*this,host_);
        if (aim!=Result::Completed) return lastResult_=aim;
        // Native shared Update reads the phase after the complete Aim call.
        if (state_.phase==0) state_.Stage0ForAnalysis(*this,host_);
        else if (state_.phase==1)
            return lastResult_=state_.WithdrawlStage1ForAnalysis(*this,host_);
        return lastResult_=Result::Completed;
    }
    bool wxIceGargoyleWithdrawlAIAction::vfunc_24() noexcept
    { return UpdateForAnalysis()==Result::Completed; }
}

