#include "wxFlyingWanderAIAction.h"
#include "Code/Sparkplug/spNode.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxFlyingWanderAIActionHost* factoryHost=nullptr;
        auto factoryProfile=wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64;
        std::unique_ptr<spBaseObject> CreateFlyingWander()
        {
            if (!factoryHost) throw std::logic_error("FlyingWander requires a stable root/geometry host");
            return std::make_unique<wxFlyingWanderAIAction>(*factoryHost,factoryProfile);
        }
        const spRTTIRecord record{wxFlyingWanderAIAction::ClassID,wxAIAction::ClassID,"wxFlyingWanderAIAction",
            &wxAIAction::StaticRTTI(),&CreateFlyingWander,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxFlyingWanderAIAction::wxFlyingWanderAIAction(wxFlyingWanderAIActionHost& host,
        wxPerceptionNumericProfileForAnalysis profile):host_(host),numericProfile_(profile)
    {
        if (profile!=wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64 &&
            profile!=wxPerceptionNumericProfileForAnalysis::ExactSmallInteger)
            throw std::invalid_argument("FlyingWander query profile is unknown");
        const auto first=host_.ReadDefaultVectorWordsForAnalysis();
        ownWords_[0]=first[0];ownWords_[1]=first[1];ownWords_[2]=first[2];
        ownWords_[3]=0x43960000; //300.0f
        ownWords_[4]=ownWords_[5]=0;
        const auto second=host_.ReadDefaultVectorWordsForAnalysis();
        ownWords_[6]=second[0];ownWords_[7]=second[1];ownWords_[8]=second[2];
        ownWords_[9]=ownWords_[10]=ownWords_[11]=0;
        // PC5B5220 constructs a distinct actual Node, establishes own ref,
        // then attaches to the current root. Shared Node code owns the graph.
        helperNode_=std::make_shared<spNode>();
        auto& root=host_.ReadSceneRootForAnalysis();
        if (root.GetSceneForAnalysis()!=nullptr)
            throw std::logic_error("FlyingWander non-null Scene registration is unqualified");
        if (!root.AttachChildForAnalysis(helperNode_))
            throw std::logic_error("FlyingWander root attachment refused by shared Node guard");
    }
    wxFlyingWanderAIAction::~wxFlyingWanderAIAction()
    {
        if (helperNode_)
        {
            // PC5B5350 detaches before dropping its own reference, then the
            // common AIAction destructor runs. External retained refs survive.
            (void)host_.ReadSceneRootForAnalysis().DetachChildForAnalysis(*helperNode_);
            helperNode_.reset();
        }
    }
    void wxFlyingWanderAIAction::SetFactoryHostForAnalysis(wxFlyingWanderAIActionHost* host,
        wxPerceptionNumericProfileForAnalysis profile) noexcept { factoryHost=host;factoryProfile=profile; }
    const spRTTIRecord& wxFlyingWanderAIAction::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxFlyingWanderAIAction::vfunc_18() const noexcept { return record; }
    bool wxFlyingWanderAIAction::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* target=dynamic_cast<wxFlyingWanderAIAction*>(&destination);
        if (!target || !wxAIAction::vfunc_14(destination,manager)) return false;
        // PC5B5140 reads/writes this exact order, not a replacement memcpy.
        for (const unsigned index:{10U,11U,4U,5U,0U,1U,2U,3U,9U,6U,7U,8U})
            target->ownWords_[index]=ownWords_[index];
        // byte3D8 and owned Node3DC are NOT copied.
        return true;
    }
    std::unique_ptr<spBaseObject> wxFlyingWanderAIAction::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxFlyingWanderAIAction>(host_,numericProfile_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    wxFlyingWanderQueryResultForAnalysis wxFlyingWanderAIAction::QueryForAnalysis() noexcept
    {
        using Result=wxFlyingWanderQueryResultForAnalysis;
        if (!host_.ReadOwnerPermissionForAnalysis(GetOwnerForAnalysis())) return Result::PermissionDisabled;
        if (!host_.ReadPerceptionForAnalysis(GetOwnerForAnalysis()).FindNearestForAnalysis(numericProfile_))
            return Result::NoTarget;
        host_.SelectActionForAnalysis(GetOwnerForAnalysis(),2,0);
        return Result::SelectorRequested;
    }
    void wxFlyingWanderAIAction::vfunc_38() noexcept { (void)QueryForAnalysis(); }
}
