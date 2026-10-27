#include "wxMinotaurAttackAIAction.h"
#include "wxPerception.h"
#include "wxGameTimer.h"
#include "wxCharacterMoveCtrl.h"
#include "Analysis/PC/wxMinotaurAttackMath.h"
#include "Analysis/PC/wxPlanarAngleMath.h"
#include "Code/Sparkplug/spNode.h"
#include <bit>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    using Result=wxMinotaurAttackResultForAnalysis;
    namespace
    {
        wxMinotaurAttackAIActionHost* factoryHost=nullptr;
        std::unique_ptr<spBaseObject> Create()
        {
            if (!factoryHost) throw std::logic_error("MinotaurAttack requires borrowed data and real warm services");
            return std::make_unique<wxMinotaurAttackAIAction>(*factoryHost);
        }
        const spRTTIRecord record{wxMinotaurAttackAIAction::ClassID,wxAIAction::ClassID,"wxMinotaurAttackAIAction",
            &wxAIAction::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxMinotaurAttackAIAction::wxMinotaurAttackAIAction(wxMinotaurAttackAIActionHost& host,wxMinotaurAttackProfileForAnalysis profile) noexcept
        :host_(host),profile_(profile) {}
    void wxMinotaurAttackAIAction::SetFactoryHostForAnalysis(wxMinotaurAttackAIActionHost* host) noexcept { factoryHost=host; }
    const spRTTIRecord& wxMinotaurAttackAIAction::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxMinotaurAttackAIAction::vfunc_18() const noexcept { return record; }
    bool wxMinotaurAttackAIAction::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* target=dynamic_cast<wxMinotaurAttackAIAction*>(&destination);
        if (!target || !wxAIAction::vfunc_14(destination,manager)) return false;
        auto& d=target->state_;
        d.target=state_.target;d.phase=state_.phase;d.deadline=state_.deadline;d.randomWord3B4=state_.randomWord3B4;
        d.word3B8=state_.word3B8;d.byte3BC=state_.byte3BC;d.command=state_.command;
        // Own byte3BD and unwritten word3C4 remain destination-local.
        return true;
    }
    std::unique_ptr<spBaseObject> wxMinotaurAttackAIAction::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxMinotaurAttackAIAction>(host_,profile_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    Result wxMinotaurAttackAIAction::EnterForAnalysis() noexcept
    {
        host_.WriteCommandByteForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()),0x1D,1);
        auto* target=host_.ReadPerceptionForAnalysis(GetOwnerForAnalysis()).FindNearestForAnalysis(wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64);
        auto* owner=GetOwnerForAnalysis();
        state_.target=target;state_.phase=0;state_.byte3BC=0;state_.deadline=0;
        state_.command=host_.ReadCommandForAnalysis(owner);
        if (!target)
        {
            host_.SelectActionForAnalysis(owner,1,0);
            // Literal selector-entry prefix: the native selector return and
            // subsequent RNG are not qualified for the null-target input.
            return lastResult_=Result::SelectorRequested;
        }
        state_.randomWord3B4=analysis::pc::PcMinotaurAttackRandomWordForAnalysis(host_.RandomStateForAnalysis().Next());
        return lastResult_=Result::Completed;
    }
    void wxMinotaurAttackAIAction::vfunc_34_ClearForAnalysis() noexcept
    {
        host_.WriteCommandByteForAnalysis(state_.command,0x1D,0);
        host_.WriteCommandByteForAnalysis(state_.command,0x20,0);
        state_.target=nullptr;
    }
    Result wxMinotaurAttackAIAction::QueryForAnalysis() noexcept
    {
        if (host_.ReadPerceptionForAnalysis(GetOwnerForAnalysis()).FindNearestForAnalysis(wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64))
            return lastResult_=Result::Completed;
        host_.SelectActionForAnalysis(GetOwnerForAnalysis(),1,0);return lastResult_=Result::SelectorRequested;
    }
    void wxMinotaurAttackAIAction::vfunc_0C(const void* notification) noexcept
    {
        const auto* words=static_cast<const std::uint32_t*>(notification);
        if (words[0]==0x271C)
        {
            if (!state_.byte3BC) return;
            const auto payload=static_cast<const std::uint8_t*>(notification)[0x18];
            host_.WriteCommandByteForAnalysis(state_.command,0x60,std::uint8_t(payload==0));
            if (host_.ReadCommandByteForAnalysis(state_.command,0x60))
            {
                const auto now=host_.GameTimerForAnalysis().GetStateForAnalysis().currentMilliseconds;
                const auto random=host_.RandomStateForAnalysis().Next();
                state_.phase=3;state_.deadline=now+2000U+random%1001U;
            }
            else state_.phase=2;
        }
        else if (words[0]!=0x27A4) wxAIAction::vfunc_0C(notification);
    }
    Result wxMinotaurAttackAIAction::UpdateForAnalysis() noexcept
    {
        if (profile_!=wxMinotaurAttackProfileForAnalysis::PcCooldown && profile_!=wxMinotaurAttackProfileForAnalysis::PcCooldownAndFiniteMovement &&
            profile_!=wxMinotaurAttackProfileForAnalysis::PcMovementWithCapturedControllerReset)
            return lastResult_=Result::UnqualifiedUpdateProfile;
        if (state_.phase>5) { state_.phase=0;return lastResult_=Result::Completed; }
        if (state_.phase==2 && (profile_==wxMinotaurAttackProfileForAnalysis::PcCooldownAndFiniteMovement ||
            profile_==wxMinotaurAttackProfileForAnalysis::PcMovementWithCapturedControllerReset))
            return lastResult_=MovePhase2ForAnalysis();
        if (state_.phase!=3) return lastResult_=Result::UnqualifiedPhase;
        auto* capturedCommand=state_.command;
        if (host_.ReadCommandByteForAnalysis(capturedCommand,0x60))
            host_.WriteCommandByteForAnalysis(capturedCommand,0x60,
                std::uint8_t(host_.ReadCharacterState130ForAnalysis(GetOwnerForAnalysis())!=0x1C));
        host_.WriteCommandByteForAnalysis(state_.command,0x20,0);
        state_.byte3BC=0;
        const auto now=host_.GameTimerForAnalysis().GetStateForAnalysis().currentMilliseconds;
        if (now>state_.deadline)
        {
            const auto random=host_.RandomStateForAnalysis().Next();state_.phase=0;
            state_.randomWord3B4=analysis::pc::PcMinotaurAttackRandomWordForAnalysis(random);
        }
        return lastResult_=Result::Completed;
    }
    Result wxMinotaurAttackAIAction::MovePhase2ForAnalysis() noexcept
    {
        host_.WriteCommandByteForAnalysis(state_.command,0x20,0);state_.byte3BC=0;
        const auto origin=host_.ReadNodeForAnalysis(GetOwnerForAnalysis()).GetWorldPositionForAnalysis();
        if (!state_.target) return Result::UnqualifiedTargetData;
        const auto target=host_.ReadNodeForAnalysis(state_.target).GetWorldPositionForAnalysis();
        bool outside;
        if (!analysis::pc::PcMinotaurAttackOutsideRangeForAnalysis(origin,target,outside)) return Result::UnqualifiedNumericDomain;
        if (outside)
        {
            state_.phase=3;host_.WriteCommandWordForAnalysis(state_.command,4,0);
            if (profile_==wxMinotaurAttackProfileForAnalysis::PcMovementWithCapturedControllerReset)
            {
                // Fresh owner read follows the motion store, as in5B927A.
                if (auto* controller=host_.ReadMoveControllerForAnalysis(GetOwnerForAnalysis()))
                { controller->ResetFieldsForAnalysis();return Result::Completed; }
            }
            // Old profiles and missing typed post-state binding keep the
            // explicit boundary; no callback replaces a controller body.
            return Result::UnqualifiedControllerReset;
        }
        float yaw;
        // PC pushes origin, then target: the helper's first argument is
        // target and its second is origin, so this phase heads away.
        if (!analysis::pc::PcPlanarPositionYawForAnalysis(target,origin,yaw)) return Result::UnqualifiedNumericDomain;
        host_.WriteCommandWordForAnalysis(state_.command,4,0x3F800000);
        host_.WriteCommandWordForAnalysis(state_.command,8,std::bit_cast<std::uint32_t>(yaw));
        return Result::Completed;
    }
    bool wxMinotaurAttackAIAction::vfunc_24() noexcept { return UpdateForAnalysis()==Result::Completed; }
}
