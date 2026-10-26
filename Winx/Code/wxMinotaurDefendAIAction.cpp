#include "wxMinotaurDefendAIAction.h"
#include <bit>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxMinotaurDefendAIActionHost* factoryHost = nullptr;
        auto factoryPlatform = wxMinotaurDefendPlatformForAnalysis::PC;
        std::unique_ptr<spBaseObject> CreateDefend()
        {
            if (factoryHost == nullptr) throw std::logic_error("wxMinotaurDefendAIAction requires a factory host");
            return std::make_unique<wxMinotaurDefendAIAction>(*factoryHost, factoryPlatform);
        }
        const spRTTIRecord record{wxMinotaurDefendAIAction::ClassID, wxAIAction::ClassID,
            "wxMinotaurDefendAIAction", &wxAIAction::StaticRTTI(), &CreateDefend, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxMinotaurDefendAIAction::wxMinotaurDefendAIAction(wxMinotaurDefendAIActionHost& host,
        const wxMinotaurDefendPlatformForAnalysis platform) noexcept : host_(host), platform_(platform) {}
    void wxMinotaurDefendAIAction::SetFactoryHostForAnalysis(wxMinotaurDefendAIActionHost* host,
        const wxMinotaurDefendPlatformForAnalysis platform) noexcept { factoryHost = host; factoryPlatform = platform; }
    const spRTTIRecord& wxMinotaurDefendAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxMinotaurDefendAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxMinotaurDefendAIAction::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxMinotaurDefendAIAction>(host_, platform_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool wxMinotaurDefendAIAction::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxMinotaurDefendAIAction*>(&destination);
        if (target == nullptr || !wxAIAction::vfunc_14(destination, manager)) return false;
        target->target_ = target_;
        target->state_ = state_;
        target->deadline_ = deadline_;
        target->command_ = command_;
        return true;
    }
    wxPerceptionNumericProfileForAnalysis wxMinotaurDefendAIAction::PerceptionProfileForAnalysis() const noexcept
    {
        return platform_ == wxMinotaurDefendPlatformForAnalysis::PC ?
            wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64 :
            wxPerceptionNumericProfileForAnalysis::ExactSmallInteger;
    }
    void wxMinotaurDefendAIAction::vfunc_30() noexcept
    {
        target_ = host_.ReadPerceptionForAnalysis(GetOwnerForAnalysis()).FindNearestForAnalysis(PerceptionProfileForAnalysis());
        state_ = 0;
        const auto now = host_.GameTimerForAnalysis().GetStateForAnalysis().currentMilliseconds;
        const auto random = host_.RandomStateForAnalysis().Next();
        auto* const ownerAfterRandom = GetOwnerForAnalysis();
        deadline_ = now + 3000u + random % 1501u;
        command_ = host_.ReadCommandForAnalysis(ownerAfterRandom);
        if (target_ == nullptr) host_.SelectActionForAnalysis(ownerAfterRandom, 1, 0);
    }
    void wxMinotaurDefendAIAction::vfunc_34_ClearForAnalysis() noexcept
    {
        host_.WriteCommandByteForAnalysis(command_, 0x1D, 0);
        host_.WriteCommandByteForAnalysis(command_, 0x1F, 0);
        target_ = nullptr;
    }
    void wxMinotaurDefendAIAction::vfunc_38() noexcept
    {
        if (host_.ReadPerceptionForAnalysis(GetOwnerForAnalysis()).FindNearestForAnalysis(PerceptionProfileForAnalysis()) == nullptr)
            host_.SelectActionForAnalysis(GetOwnerForAnalysis(), 1, 0);
    }
    void wxMinotaurDefendAIAction::vfunc_0C(const void* notification) noexcept
    {
        const auto code = static_cast<const wxAIActionMessageForAnalysis*>(notification)->code;
        if (code == 0x2717)
        {
            const auto now = host_.GameTimerForAnalysis().GetStateForAnalysis().currentMilliseconds;
            const auto remaining = std::bit_cast<std::int32_t>(deadline_ - now);
            if (remaining < 3000)
            {
                // PC reuses its captured now. PS2 loads the global timer and
                // its current field again after deciding to extend the deadline.
                const auto extensionNow = platform_ == wxMinotaurDefendPlatformForAnalysis::PS2 ?
                    host_.GameTimerForAnalysis().GetStateForAnalysis().currentMilliseconds : now;
                deadline_ = extensionNow + 3000u;
            }
        }
        else if (code != 0x27A4) wxAIAction::vfunc_0C(notification);
    }
    wxMinotaurDefendGateResultForAnalysis wxMinotaurDefendAIAction::UpdateGateForAnalysis() noexcept
    {
        const auto now = host_.GameTimerForAnalysis().GetStateForAnalysis().currentMilliseconds;
        if (now > deadline_)
        {
            host_.SelectActionForAnalysis(GetOwnerForAnalysis(), 2, 0);
            lastGate_ = wxMinotaurDefendGateResultForAnalysis::ExpiredSelector;
        }
        else if (state_ == 0) lastGate_ = wxMinotaurDefendGateResultForAnalysis::UnqualifiedState0;
        else if (state_ == 1) lastGate_ = wxMinotaurDefendGateResultForAnalysis::UnqualifiedState1;
        else
        {
            state_ = 1;
            lastGate_ = wxMinotaurDefendGateResultForAnalysis::StateReset;
        }
        return lastGate_;
    }
    bool wxMinotaurDefendAIAction::vfunc_24() noexcept
    {
        const auto result = UpdateGateForAnalysis();
        if (result == wxMinotaurDefendGateResultForAnalysis::UnqualifiedState0 ||
            result == wxMinotaurDefendGateResultForAnalysis::UnqualifiedState1)
        {
            host_.ObserveUnqualifiedStateForAnalysis(*this, state_);
            return false; // analytical refusal, never a native false claim
        }
        return true;
    }
}
