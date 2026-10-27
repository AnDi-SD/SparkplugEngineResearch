#include "wxHurtAIAction.h"
#include "wxGameTimer.h"
#include "wxPerception.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/wxHurtRangeMath.h"
#include <bit>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxHurtAIActionHost* factoryHost=nullptr;
        std::unique_ptr<spBaseObject> CreateHurt()
        {
            if (!factoryHost) throw std::logic_error("HurtAIAction requires a host");
            return std::make_unique<wxHurtAIAction>(*factoryHost);
        }
        const spRTTIRecord record{wxHurtAIAction::ClassID,wxAIAction::ClassID,"wxHurtAIAction",
            &wxAIAction::StaticRTTI(),&CreateHurt,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxHurtAIAction::wxHurtAIAction(wxHurtAIActionHost& host,wxHurtPlatformForAnalysis platform,
        wxHurtUpdateProfileForAnalysis profile) noexcept :host_(host),platform_(platform),profile_(profile) {}
    void wxHurtAIAction::SetFactoryHostForAnalysis(wxHurtAIActionHost* host) noexcept { factoryHost=host; }
    const spRTTIRecord& wxHurtAIAction::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxHurtAIAction::vfunc_18() const noexcept { return record; }
    bool wxHurtAIAction::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        if (!dynamic_cast<wxHurtAIAction*>(&destination)) return false;
        // PC5AA030/PS2250920 perform base Copy and literal completion. The
        // own latch is NOT copied; the destination retains its existing byte.
        return wxAIAction::vfunc_14(destination,manager);
    }
    std::unique_ptr<spBaseObject> wxHurtAIAction::vfunc_10(spCloneManager& manager) const
    {
        auto result=std::make_unique<wxHurtAIAction>(host_,platform_,profile_);
        manager.RegisterCloneForAnalysis(*this,*result);
        return vfunc_14(*result,manager)?std::move(result):nullptr;
    }
    void wxHurtAIAction::EnterForAnalysis(std::uint32_t argument) noexcept
    {
        auto* command=host_.ReadCommandForAnalysis(GetOwnerForAnalysis());
        if (!command) return; // preserves an existing latch as in both originals
        host_.WriteCommandByteForAnalysis(command,0x5C,1);
        host_.WriteCommandByteForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()),0x60,argument!=0);
        host_.WriteCommandWordForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()),4,0);
        latch_=0;
    }
    void wxHurtAIAction::vfunc_34_ClearForAnalysis() noexcept
    { host_.WriteCommandByteForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()),0x5C,0); }
    wxHurtResultForAnalysis wxHurtAIAction::ExtendRangeForAnalysis() noexcept
    {
        using Result=wxHurtResultForAnalysis;
        if (platform_!=wxHurtPlatformForAnalysis::PC || profile_!=wxHurtUpdateProfileForAnalysis::PcSmallIntegerRange)
            return Result::UnqualifiedRangeHelper;
        const auto origin=host_.ReadOwnerNodeForAnalysis(GetOwnerForAnalysis()).GetPositionForAnalysis();
        const auto player=host_.ReadPlayerNodeForAnalysis().GetPositionForAnalysis();
        auto& firstPerception=host_.ReadPerceptionForAnalysis(GetOwnerForAnalysis());
        bool extend;float range;
        if (!analysis::pc::HurtRangeForAnalysis(origin,player,
            std::bit_cast<float>(firstPerception.StateForAnalysis().word3C),extend,range))
            return Result::UnqualifiedNumericDomain;
        if (!extend) return Result::Completed;
        host_.ReadPerceptionForAnalysis(GetOwnerForAnalysis()).StateForAnalysis().word3C=std::bit_cast<std::uint32_t>(range);
        auto& refreshed=host_.ReadPerceptionForAnalysis(GetOwnerForAnalysis());
        const auto result=refreshed.RefreshBoundedPcForAnalysis(host_.GameTimerForAnalysis());
        if (result==wxPerceptionPcRefreshResultForAnalysis::UnqualifiedOwnerScan ||
            result==wxPerceptionPcRefreshResultForAnalysis::UnqualifiedNativeDeadline)
            return Result::UnqualifiedPerceptionRefresh;
        // PC5AA070 tail-calls Perception; the enclosing Update ignores its
        // bool return, including the real paused/native-false case.
        return Result::Completed;
    }
    wxHurtResultForAnalysis wxHurtAIAction::UpdateForAnalysis() noexcept
    {
        using Result=wxHurtResultForAnalysis;
        const bool hadLatch=latch_!=0;
        const auto state=host_.ReadCharacterStateForAnalysis(GetOwnerForAnalysis());
        if (!hadLatch)
        {
            if (state==0xA)
            {
                host_.ResetCommandForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()));
                latch_=1;
            }
            return lastResult_=Result::Completed;
        }
        if (state==0xA) return lastResult_=Result::Completed;
        const auto result=ExtendRangeForAnalysis();
        if (result!=Result::Completed) return lastResult_=result;
        latch_=0;
        host_.SelectActionForAnalysis(GetOwnerForAnalysis(),2,0);
        return lastResult_=Result::SelectorRequested;
    }
    bool wxHurtAIAction::vfunc_24() noexcept
    {
        const auto result=UpdateForAnalysis();
        return result==wxHurtResultForAnalysis::Completed || result==wxHurtResultForAnalysis::SelectorRequested;
    }
}
