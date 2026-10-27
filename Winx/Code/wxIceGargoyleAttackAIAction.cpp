#include "wxIceGargoyleAttackAIAction.h"
#include "wxTargetManager.h"
#include "wxGameTimer.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/wxIceGargoyleAttackMath.h"
#include "Analysis/PC/wxPlanarAngleMath.h"
#include "Analysis/PC/spNodeTransformMath.h"
#include <bit>
#include <stdexcept>
#include <typeinfo>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    using Result=wxIceGargoyleAttackResultForAnalysis;
    namespace math=winx::analysis::pc::ice_gargoyle_attack_math;
    namespace
    {
        wxIceGargoyleAttackAIActionHost* factoryHost=nullptr;
        std::unique_ptr<spBaseObject> Create()
        {
            if (!factoryHost) throw std::logic_error("GargoyleAttack requires stable borrowed data/services");
            return std::make_unique<wxIceGargoyleAttackAIAction>(*factoryHost);
        }
        const spRTTIRecord record{wxIceGargoyleAttackAIAction::ClassID,wxAIAction::ClassID,"wxIceGargoyleAttackAIAction",
            &wxAIAction::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        bool PlainNode(const spNode& node) noexcept
        {
            return typeid(node)==typeid(spNode) && !node.GetParentForAnalysis() &&
                !node.GetSceneForAnalysis() && node.GetChildCountForAnalysis()==0 && node.GetCollisionCountForAnalysis()==0;
        }
        bool Delta(float dt) noexcept { return math::Bounded(dt,1.0f); }
    }
    wxIceGargoyleAttackAIAction::wxIceGargoyleAttackAIAction(wxIceGargoyleAttackAIActionHost& host,
        wxIceGargoyleAttackProfileForAnalysis profile,wxPerceptionNumericProfileForAnalysis perception)
        :host_(host),profile_(profile),perceptionProfile_(perception)
    {
        if (perception!=wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64 &&
            perception!=wxPerceptionNumericProfileForAnalysis::ExactSmallInteger)
            throw std::invalid_argument("GargoyleAttack Perception profile is unknown");
        state_.waypoint=host_.ReadDefaultVectorWordsForAnalysis();
    }
    void wxIceGargoyleAttackAIAction::SetFactoryHostForAnalysis(wxIceGargoyleAttackAIActionHost* host) noexcept { factoryHost=host; }
    const spRTTIRecord& wxIceGargoyleAttackAIAction::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxIceGargoyleAttackAIAction::vfunc_18() const noexcept { return record; }
    bool wxIceGargoyleAttackAIAction::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* target=dynamic_cast<wxIceGargoyleAttackAIAction*>(&destination);
        if (!target || !wxAIAction::vfunc_14(destination,manager)) return false;
        auto& t=target->state_;
        t.cachedNode=state_.cachedNode;t.phase=state_.phase;t.time=state_.time;
        t.waypoint[0]=state_.waypoint[0];t.waypoint[1]=state_.waypoint[1];t.waypoint[2]=state_.waypoint[2];
        t.threshold=state_.threshold;t.byte3D0=state_.byte3D0;t.byte3D1=state_.byte3D1;
        t.word3C8=state_.word3C8;t.word3CC=state_.word3CC;t.byte3D2=state_.byte3D2;
        // Toggle/direction/speed/latch remain destination-local, as in5AF370.
        return true;
    }
    std::unique_ptr<spBaseObject> wxIceGargoyleAttackAIAction::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxIceGargoyleAttackAIAction>(host_,profile_,perceptionProfile_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    Result wxIceGargoyleAttackAIAction::RefreshTargetForAnalysis() noexcept
    {
        auto* manager=wxTargetManager::GetInstanceForAnalysis();
        if (!manager) return Result::UnqualifiedColdTargetManager;
        const auto identity=manager->CurrentWordsForAnalysis()[2];spNode* node=nullptr;
        if (identity)
        {
            const auto value=host_.ReadBorrowedRecordNode18ForAnalysis(identity);
            if (!value) return Result::UnqualifiedTargetRecordData;
            node=*value;
        }
        state_.cachedNode=node;return Result::Completed;
    }
    Result wxIceGargoyleAttackAIAction::EnterForAnalysis() noexcept
    {
        const auto target=RefreshTargetForAnalysis();
        if (target!=Result::Completed) return lastResult_=target;
        state_.phase=2;state_.time=0;state_.toggle=0;
        if (!state_.cachedNode)
        { host_.SelectActionForAnalysis(GetOwnerForAnalysis(),0,0);return lastResult_=Result::SelectorRequested; }
        return lastResult_=Result::Completed;
    }
    void wxIceGargoyleAttackAIAction::vfunc_34_ClearForAnalysis() noexcept
    {
        host_.WriteCommandByteForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()),0x20,0);
        host_.WriteCommandByteForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()),0x21,0);
        state_.cachedNode=nullptr;
    }
    Result wxIceGargoyleAttackAIAction::QueryForAnalysis() noexcept
    {
        if (host_.ReadPerceptionForAnalysis(GetOwnerForAnalysis()).FindNearestForAnalysis(perceptionProfile_))
            return lastResult_=Result::Completed;
        host_.SelectActionForAnalysis(GetOwnerForAnalysis(),0,0);return lastResult_=Result::SelectorRequested;
    }
    Result wxIceGargoyleAttackAIAction::OrientForAnalysis() noexcept
    {
        if (!state_.cachedNode) return Result::UnqualifiedCachedNode;
        // PC5AFCC0 reads target WORLD X/Z before owner LOCAL X/Z.
        const auto target=state_.cachedNode->GetWorldPositionForAnalysis();
        const auto origin=host_.ReadOwnerNodeForAnalysis(GetOwnerForAnalysis()).GetPositionForAnalysis();
        math::Vector direction;
        if (!math::Direction(origin,target,direction,true)) return Result::UnqualifiedNumericDomain;
        const auto upWords=host_.ReadUpVectorWordsForAnalysis();
        if (upWords!=std::array<std::uint32_t,3>{0,0x3F800000,0}) return Result::UnqualifiedNumericDomain;
        const math::Vector up{0,1,0};
        // Canonical up differs in Y from every planar direction, so the
        // uninitialized coincident-input branch of420470 cannot be entered.
        const auto right=sparkplug::evidence::pc::node_math::Cross(up,direction);
        const spNode::Matrix3 matrix{right[0],right[1],right[2],up[0],up[1],up[2],direction[0],direction[1],direction[2]};
        (void)winx::analysis::pc::PcPlanarVectorYawForAnalysis(direction); //actual597140 result is discarded
        auto& destination=host_.ReadOwnerNodeForAnalysis(GetOwnerForAnalysis());
        if (!PlainNode(destination)) return Result::UnqualifiedNodeWorld;
        destination.SetOrientationForAnalysis(matrix);destination.MarkLocalTransformDirtyForAnalysis();
        if (!destination.UpdateWorldForAnalysis()) return Result::UnqualifiedNodeWorld;
        return Result::Completed;
    }
    Result wxIceGargoyleAttackAIAction::Stage0ForAnalysis() noexcept
    {
        if (!state_.time)
        {
            state_.latch=0;state_.speed=0;
            host_.ResetCommandForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()));
            const auto now=host_.GameTimerForAnalysis().GetStateForAnalysis().currentMilliseconds;
            const auto random=host_.RandomStateForAnalysis().Next();
            state_.time=now+500U+random%1501U;return Result::Completed;
        }
        const auto now=host_.GameTimerForAnalysis().GetStateForAnalysis().currentMilliseconds;
        if (now<=state_.time) return Result::Completed;
        const auto random=host_.RandomStateForAnalysis().Next();
        const float threshold=std::bit_cast<float>(state_.threshold);
        if (!std::isfinite(threshold)) return Result::UnqualifiedNumericDomain;
        // u32 times exact2^-32 and a float comparison have no binary64 loss.
        state_.phase=double(random)*0x1p-32 < double(threshold)?1:2;state_.time=0;
        return Result::Completed;
    }
    Result wxIceGargoyleAttackAIAction::Stage1ForAnalysis() noexcept
    {
        if (!state_.time)
        {
            host_.WriteCommandByteForAnalysis(host_.ReadCommandForAnalysis(GetOwnerForAnalysis()),0x21,1);
            state_.time=host_.GameTimerForAnalysis().GetStateForAnalysis().currentMilliseconds;
            return Result::Completed;
        }
        auto* owner=GetOwnerForAnalysis();
        if ((host_.ReadCharacterFlags140ForAnalysis(owner)&0x7F80)==0x480)
        {
            host_.WriteCommandByteForAnalysis(host_.ReadCommandForAnalysis(owner),0x21,0);
            state_.phase=host_.RandomStateForAnalysis().Next()<0x80000000U?0:2;
            // Native retains time, including the transition to phase2.
        }
        return Result::Completed;
    }
    Result wxIceGargoyleAttackAIAction::Stage2ForAnalysis() noexcept
    {
        const auto origin=host_.ReadOwnerNodeForAnalysis(GetOwnerForAnalysis()).GetPositionForAnalysis();
        if (!state_.time)
        {
            if (state_.toggle)
                state_.waypoint=host_.ReadWaypointWordsForAnalysis(host_.RandomStateForAnalysis().Next()&3U);
            else
            {
                constexpr std::array<float,3> factors{0x1.9p-25f,0x1.2cp-25f,0x1.0ep-23f}; //200/2^32,150/2^32,540/2^32
                constexpr std::array<float,3> biases{1650,1500,460};
                for (unsigned axis=0;axis!=3;++axis)
                {
                    const auto random=host_.RandomStateForAnalysis().Next();
                    state_.waypoint[axis]=std::bit_cast<std::uint32_t>(math::RandomCoordinate(random,factors[axis],biases[axis]));
                }
            }
            state_.time=host_.GameTimerForAnalysis().GetStateForAnalysis().currentMilliseconds;
            math::Vector direction;
            if (!math::Direction(origin,math::Floats(state_.waypoint),direction,false)) return Result::UnqualifiedNumericDomain;
            state_.direction=math::Words(direction);state_.speed=0;state_.toggle=state_.toggle==0;
            return Result::Completed;
        }
        bool close;
        if (!math::SquaredLess(origin,math::Floats(state_.waypoint),400.0f,close)) return Result::UnqualifiedNumericDomain;
        if (close) { state_.phase=0;state_.time=0;return Result::Completed; }
        if (!state_.speed) return Result::UnqualifiedSpeed;
        float speed=std::bit_cast<float>(*state_.speed);
        if (!math::Bounded(speed,8192.0f)) return Result::UnqualifiedNumericDomain;
        if (speed<400.0f)
        {
            const float dt=host_.GameTimerForAnalysis().GetStateForAnalysis().deltaSeconds;
            if (!Delta(dt)) return Result::UnqualifiedNumericDomain;
            state_.speed=std::bit_cast<std::uint32_t>(math::Accelerate(speed,dt,200.0f));
        }
        const float dt=host_.GameTimerForAnalysis().GetStateForAnalysis().deltaSeconds;
        if (!Delta(dt)) return Result::UnqualifiedNumericDomain;
        speed=std::bit_cast<float>(*state_.speed);
        auto& destination=host_.ReadOwnerNodeForAnalysis(GetOwnerForAnalysis());
        const auto direction=math::Floats(state_.direction);
        if (!PlainNode(destination)) return Result::UnqualifiedNodeWorld;
        if (!math::Bounded(direction,4.0f)) return Result::UnqualifiedNumericDomain;
        destination.SetPositionForAnalysis(math::MoveWaypoint(origin,direction,speed,dt));destination.MarkLocalTransformDirtyForAnalysis();
        return Result::Completed;
    }
    Result wxIceGargoyleAttackAIAction::Stage3ForAnalysis() noexcept
    {
        if (!state_.speed) return Result::UnqualifiedSpeed;
        const float speed=std::bit_cast<float>(*state_.speed);
        const float firstDelta=host_.GameTimerForAnalysis().GetStateForAnalysis().deltaSeconds;
        if (!math::Bounded(speed,8192.0f) || !Delta(firstDelta)) return Result::UnqualifiedNumericDomain;
        state_.speed=std::bit_cast<std::uint32_t>(math::Accelerate(speed,firstDelta,-980.0f));
        const auto origin=host_.ReadOwnerNodeForAnalysis(GetOwnerForAnalysis()).GetPositionForAnalysis();
        const float secondDelta=host_.GameTimerForAnalysis().GetStateForAnalysis().deltaSeconds;
        if (!math::Bounded(origin) || !Delta(secondDelta)) return Result::UnqualifiedNumericDomain;
        auto& destination=host_.ReadOwnerNodeForAnalysis(GetOwnerForAnalysis());
        if (!PlainNode(destination)) return Result::UnqualifiedNodeWorld;
        destination.SetPositionForAnalysis({origin[0],math::FallY(origin[1],std::bit_cast<float>(*state_.speed),secondDelta),origin[2]});
        destination.MarkLocalTransformDirtyForAnalysis();
        const bool hadLatch=state_.latch!=0;
        auto* owner=GetOwnerForAnalysis();
        const auto characterState=host_.ReadCharacterState130ForAnalysis(owner);
        if (!hadLatch)
        {
            host_.WriteCommandByteForAnalysis(host_.ReadCommandForAnalysis(owner),0x5C,characterState==0xA?0:1);
            if (characterState==0xA) state_.latch=1;
        }
        else if (characterState!=0xA)
        { state_.time=0;state_.speed=0;state_.latch=0;state_.phase=2; }
        return Result::Completed;
    }
    Result wxIceGargoyleAttackAIAction::UpdateForAnalysis() noexcept
    {
        if (profile_!=wxIceGargoyleAttackProfileForAnalysis::PcFiniteGeometry) return lastResult_=Result::UnqualifiedUpdateProfile;
        auto result=RefreshTargetForAnalysis();if (result!=Result::Completed) return lastResult_=result;
        result=OrientForAnalysis();if (result!=Result::Completed) return lastResult_=result;
        switch (state_.phase)
        {
        case 0:return lastResult_=Stage0ForAnalysis();
        case 1:return lastResult_=Stage1ForAnalysis();
        case 2:return lastResult_=Stage2ForAnalysis();
        case 3:return lastResult_=Stage3ForAnalysis();
        default:return lastResult_=Result::Completed;
        }
    }
    bool wxIceGargoyleAttackAIAction::vfunc_24() noexcept { return UpdateForAnalysis()==Result::Completed; }
    Result wxIceGargoyleAttackAIAction::NotifyForAnalysis(const void* notification) noexcept
    {
        const auto* words=static_cast<const std::uint32_t*>(notification);
        if (words[0]!=0x2717) { wxAIAction::vfunc_0C(notification);return lastResult_=Result::Completed; }
        if (state_.phase==3) return lastResult_=Result::Completed;
        // Valid2717 frames include word18 even in the no-brain domain.
        const auto payload=words[6];(void)payload;
        if (host_.ReadCharacterBrain138ForAnalysis(GetOwnerForAnalysis())) return lastResult_=Result::UnqualifiedBrainDelivery;
        const float threshold=std::bit_cast<float>(state_.threshold);
        if (!std::isfinite(threshold)) return lastResult_=Result::UnqualifiedNumericDomain;
        const auto updated=math::NotifyThreshold(threshold);
        state_.speed=0;state_.time=0;state_.phase=3;state_.threshold=std::bit_cast<std::uint32_t>(updated.stored);
        if (updated.clamp) state_.threshold=0x3F4CCCCD;
        return lastResult_=Result::Completed;
    }
    void wxIceGargoyleAttackAIAction::vfunc_0C(const void* notification) noexcept { (void)NotifyForAnalysis(notification); }
    Result wxIceGargoyleAttackAIAction::CheckHeightForAnalysis() noexcept
    {
        if (!state_.cachedNode) return lastResult_=Result::UnqualifiedCachedNode;
        const float targetY=state_.cachedNode->GetWorldPositionForAnalysis()[1];auto* owner=GetOwnerForAnalysis();
        const float upper=std::bit_cast<float>(host_.ReadOwnerWordForAnalysis(owner,0x1B4));
        if (!std::isfinite(targetY) || !std::isfinite(upper)) return lastResult_=Result::UnqualifiedNumericDomain;
        if (targetY>=upper) return lastResult_=Result::Completed;
        const float lower=std::bit_cast<float>(host_.ReadOwnerWordForAnalysis(owner,0x1CC));
        if (!std::isfinite(lower)) return lastResult_=Result::UnqualifiedNumericDomain;
        if (targetY<=lower) return lastResult_=Result::Completed;
        host_.SelectActionForAnalysis(owner,5,0);return lastResult_=Result::SelectorRequested;
    }
}
