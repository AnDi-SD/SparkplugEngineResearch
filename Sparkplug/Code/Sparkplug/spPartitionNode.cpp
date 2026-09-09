#include "spPartitionNode.h"
#include "spPartitionSystem.h"
#include "spPartitionRenderable.h"
#include "spZone.h"
#include "spZonePortal.h"
#include "spStaticRenderObject.h"
#include "spCollisionInfo.h"
#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePartitionNode()
        {
            return std::make_unique<spPartitionNode>();
        }
        const spRTTIRecord Record{spPartitionNode::ClassID, spBaseObject::ClassID,
                                  "spPartitionNode",        &spBaseObject::StaticRTTI(),
                                  &CreatePartitionNode,     nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    } // namespace
    spPartitionNode::spPartitionNode()=default;
    spPartitionNode::~spPartitionNode()
    {
        // Native4264D0 first removes reciprocal borrowed collision links.
        while(!collisions_.empty())
        {
            collisions_.back()->RemovePartitionForAnalysis(this,false);
            collisions_.pop_back();
        }
        payload_.reset();
        for(auto& child:children_)child.reset(); // native ascending child order
        children_.clear();
        for(auto& portal:portals_)portal.reset(); //426551..426590, before Zone
        portals_.clear();
        zone_.reset();
        for(auto& object:statics_)object.reset(); //4265CE, after Zone
        statics_.clear();
    }
    spPartitionNode::spPartitionNode(std::size_t childCount) : children_(childCount)
    {
    }
    const spRTTIRecord& spPartitionNode::StaticRTTI() noexcept
    {
        (void)Registered;
        return Record;
    }
    const spRTTIRecord& spPartitionNode::vfunc_18() const noexcept
    {
        return Record;
    }
    std::unique_ptr<spBaseObject> spPartitionNode::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPartitionNode>();
        manager.RegisterClone(*this, *clone);
        return spBaseObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    spPartitionNode* spPartitionNode::FindLeafForAnalysis(const Vector3&, bool) noexcept
    {
        return this; // PC425680 ignores both arguments
    }
    std::size_t spPartitionNode::GetChildCountForAnalysis() const noexcept
    {
        return children_.size();
    }
    spPartitionNode* spPartitionNode::GetChildForAnalysis(std::size_t index) noexcept
    {
        return index < children_.size() ? children_[index].get() : nullptr;
    }
    bool spPartitionNode::SetChildForAnalysis(std::size_t index,
                                              std::unique_ptr<spPartitionNode> child) noexcept
    {
        if (index >= children_.size())
            return false;
        if(child)child->parent_=this; // original44B7B6
        children_[index] = std::move(child);
        return true;
    }
    void spPartitionNode::SetZonePresentForAnalysis(bool present) noexcept
    {
        zonePresent_ = present;
    }
    bool spPartitionNode::HasZoneForAnalysis() const noexcept
    {
        return zone_!=nullptr || zonePresent_;
    }
    void spPartitionNode::SetZoneForAnalysis(std::shared_ptr<spZone> value) noexcept
    {zone_=std::move(value);}
    bool spPartitionNode::AppendPortalForAnalysis(std::shared_ptr<spZonePortal> value)
    {
        if(portals_.size()>=4096)return false;
        portals_.push_back(std::move(value));return true; //4266D0 retains duplicates
    }
    void spPartitionNode::SetPartitionSystemForAnalysis(spPartitionSystem* system) noexcept
    {
        system_=system;
        if(system)SetScenePointerForAnalysis(system->GetSceneForAnalysis()); //44B7F6
    }
    void spPartitionNode::SetScenePointerForAnalysis(spScene* scene) noexcept
    {
        scene_=scene;
        if(payload_)payload_->SetScenePointerForAnalysis(scene);
        for(const auto& value:statics_)if(value)value->SetScenePointerForAnalysis(scene);
        for(const auto& child:children_)if(child)child->SetScenePointerForAnalysis(scene);
    }
    bool spPartitionNode::SetPartitionRenderableForAnalysis(std::unique_ptr<spPartitionRenderable> value) noexcept
    {
        if(!value||payload_)return false; // host replacement/alias guard
        payload_=std::move(value);payload_->SetScenePointerForAnalysis(scene_);return true;
    }
    bool spPartitionNode::InsertCollisionForAnalysis(spCollisionInfo* collision)
    {
        if(!IsExactly(ClassID)||!collision||collisions_.size()>=4096
            ||collision->partitionRoots_.size()>=4096)return false;
        collision->partitionRoots_.push_back(this); //426670 ->465470 before own append
        collisions_.push_back(collision);return true;
    }
    bool spPartitionNode::InsertStaticForAnalysis(std::shared_ptr<spStaticRenderObject> value)
    {
        if(!IsExactly(ClassID)||!value||statics_.size()>=4096)return false;
        statics_.push_back(std::move(value));
        statics_.back()->SetScenePointerForAnalysis(scene_);return true; //426740
    }
    void spPartitionNode::RemoveCollisionForAnalysis(spCollisionInfo* collision,bool notify) noexcept
    {
        auto found=std::find(collisions_.begin(),collisions_.end(),collision);
        if(found==collisions_.end())return;
        *found=collisions_.back();collisions_.pop_back();
        if(notify)collision->RemovePartitionForAnalysis(this,false); //425A80 ->464F70
    }
} // namespace sparkplug::reconstruction
