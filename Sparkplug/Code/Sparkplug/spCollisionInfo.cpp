#include "spCollisionInfo.h"
#include "spNode.h"
#include "spPartitionNode.h"
#include <algorithm>
#include "Analysis/PC/spNodeTransformMath.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spCollisionInfo>();}
        const spRTTIRecord Record{spCollisionInfo::ClassID,spBaseObject::ClassID,"spCollisionInfo",
            &spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    spCollisionInfo::~spCollisionInfo()
    {
        // Native deleting path drains reciprocal spatial registrations; Node
        // clears the Node back pointer before releasing its host owner.
        while(!partitionRoots_.empty())
        {
            partitionRoots_.back()->RemoveCollisionForAnalysis(this,false);
            partitionRoots_.pop_back();
        }
    }
    void spCollisionInfo::RemovePartitionForAnalysis(spPartitionNode* root,bool notify) noexcept
    {
        auto found=std::find(partitionRoots_.begin(),partitionRoots_.end(),root);
        if(found==partitionRoots_.end())return;
        *found=partitionRoots_.back();partitionRoots_.pop_back();
        if(notify)root->RemoveCollisionForAnalysis(this,false); //464F70
    }
    const spRTTIRecord& spCollisionInfo::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spCollisionInfo::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spCollisionInfo::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spCollisionInfo>();manager.RegisterClone(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spCollisionInfo::vfunc_14(spBaseObject& destination,spCloneManager&) const
    {
        auto* target=dynamic_cast<spCollisionInfo*>(&destination);
        if(!target||target==this)return false; // reject native destructive self-copy
        std::shared_ptr<spCollisionInfo> detached;
        if(target->node_)detached=target->node_->DetachCollisionForAnalysis(*target);
        // PC464EF0 retains the SAME primitive and copies group. PRS/bounds
        // are left alone (a newly constructed clone therefore stays identity).
        target->primitive_=primitive_;target->group_=group_;return true;
    }
    bool spCollisionInfo::UpdateWorldForAnalysis() noexcept
    {
        if(!primitive_)return false;
        const bool fromNode=node_&&!node_->IsKindOf(0x912CC341); // spPartitionSystem
        if(fromNode)
        {
            position_=node_->GetWorldPositionForAnalysis();
            orientation_=node_->GetWorldOrientationForAnalysis();
            scale_=node_->GetWorldScaleForAnalysis();
        }
        boundingCenter_=evidence::pc::node_math::Transform(primitive_->GetBoundingCenterForAnalysis(),orientation_);
        for(std::size_t i=0;i<3;++i)boundingCenter_[i]+=position_[i];
        boundingRadius_=primitive_->GetBoundingRadiusForAnalysis();
        if(fromNode)primitive_->UpdateCollisionTransformForAnalysis(position_,orientation_,scale_);
        return true;
    }
}
