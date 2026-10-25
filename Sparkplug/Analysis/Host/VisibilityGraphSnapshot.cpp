#include "VisibilityGraphSnapshot.h"
#include "Code/Sparkplug/spPartitionNode.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spStaticRenderObject.h"
#include "Code/Sparkplug/spPartitionRenderable.h"
#include "Code/Sparkplug/spZone.h"
#include "Code/Sparkplug/spZonePortal.h"
#include <algorithm>
#include <utility>

namespace sparkplug::host
{
    using namespace reconstruction;
    VisibilityGraphSnapshot::Manager::SupportForAnalysis* VisibilityGraphSnapshot::Bind(
        spBaseObject* object,Manager::SupportKindForAnalysis kind)
    {
        if(!object)return nullptr;
        for(std::size_t i=0;i<bindings_.size();++i)
            if(bindings_[i].source==object)
                return bindings_[i].kind==kind?&supports_[i]:nullptr;
        if(bindings_.size()>=4096)return nullptr;
        Manager::SupportForAnalysis support;support.kind=kind;
        switch(kind)
        {
        case Manager::SupportKindForAnalysis::RenderNode:
        {
            const auto* value=dynamic_cast<const spRenderNode*>(object);if(!value)return nullptr;
            support.worldSphere=value->GetWorldBoundingSphereForAnalysis();
            support.visibilityMark=value->GetVisibilityMarkForAnalysis();
            support.enabled=value->IsEnabledForAnalysis();support.cullBypass=value->GetCullBypassForAnalysis();break;
        }
        case Manager::SupportKindForAnalysis::Static:
        {
            const auto* value=dynamic_cast<const spStaticRenderObject*>(object);if(!value)return nullptr;
            support.worldSphere=value->GetWorldBoundingSphereForAnalysis();support.visibilityMark=value->GetVisibilityMarkForAnalysis();break;
        }
        case Manager::SupportKindForAnalysis::Partition:
        {
            const auto* value=dynamic_cast<const spPartitionRenderable*>(object);if(!value)return nullptr;
            support.worldSphere=value->GetWorldBoundingSphereForAnalysis();support.visibilityMark=value->GetVisibilityMarkForAnalysis();break;
        }
        }
        bindings_.push_back({object,kind});supports_.push_back(support);return &supports_.back();
    }
    VisibilityGraphSnapshot::Status VisibilityGraphSnapshot::Build(spPartitionNode& root)
    {
        if(attempted_)return Status::AlreadyAttempted;attempted_=true;
        // Fixed reserve keeps pointers stable during construction. This is a
        // host cap, not an assertion about the original allocation strategy.
        nodes_.reserve(4096);supports_.reserve(4096);bindings_.reserve(4096);
        std::vector<spPartitionNode*> pending{&root};
        const auto enqueue=[&](spPartitionNode* value){
            if(!value)return false;
            if(std::find(pending.begin(),pending.end(),value)!=pending.end())return true;
            if(pending.size()>=4096)return false;pending.push_back(value);return true;};
        const auto zoneRoots=[&](spZone* zone){
            if(!zone)return true;for(auto* value:zone->GetRootsForAnalysis())if(!enqueue(value))return false;return true;};
        for(std::size_t at=0;at<pending.size();++at)
        {
            auto* source=pending[at];Manager::TraversalNodeForAnalysis node;
            node.spatial=source;node.visibilityMark=source->GetVisibilityMarkForAnalysis();
            if(auto* payload=source->GetPartitionRenderableForAnalysis())
            {node.payload=Bind(payload,Manager::SupportKindForAnalysis::Partition);if(!node.payload)return Status::InvalidInput;}
            for(const auto& value:source->GetStaticObjectsForAnalysis())
            {auto* support=Bind(value.get(),Manager::SupportKindForAnalysis::Static);if(!support)return Status::InvalidInput;node.statics.push_back(support);}
            for(auto* value:source->GetDynamicObjectsForAnalysis())
            {auto* support=Bind(value,Manager::SupportKindForAnalysis::RenderNode);if(!support)return Status::InvalidInput;node.dynamics.push_back(support);}
            node.occluders=source->GetOccludersForAnalysis();
            nodes_.push_back(std::move(node));
            for(std::size_t i=0;i<source->GetChildCountForAnalysis();++i)
                if(auto* child=source->GetChildForAnalysis(i))if(!enqueue(child))return Status::HostLimit;
            if(!zoneRoots(source->GetZoneForAnalysis()))return Status::HostLimit;
            for(const auto& portal:source->GetPortalsForAnalysis())
            {if(!portal)return Status::InvalidInput;if(!zoneRoots(portal->GetDestinationZone()))return Status::HostLimit;}
        }
        scene_.partitionRoot=&root;for(auto& node:nodes_)scene_.nodes.push_back(&node);
        ready_=true;return Status::Complete;
    }
    bool VisibilityGraphSnapshot::PublishMarks() noexcept
    {
        if(!ready_)return false;
        for(const auto& node:nodes_)node.spatial->SetVisibilityMarkForAnalysis(node.visibilityMark);
        for(std::size_t i=0;i<bindings_.size();++i)
        {
            const auto& binding=bindings_[i];const auto mark=supports_[i].visibilityMark;
            switch(binding.kind)
            {
            case Manager::SupportKindForAnalysis::RenderNode:static_cast<spRenderNode*>(binding.source)->SetVisibilityMarkForAnalysis(mark);break;
            case Manager::SupportKindForAnalysis::Static:static_cast<spStaticRenderObject*>(binding.source)->SetVisibilityMarkForAnalysis(mark);break;
            case Manager::SupportKindForAnalysis::Partition:static_cast<spPartitionRenderable*>(binding.source)->SetVisibilityMarkForAnalysis(mark);break;
            }
        }
        return true;
    }
    spBaseObject* VisibilityGraphSnapshot::SourceOf(const Manager::SupportForAnalysis* support) const noexcept
    {
        if(!ready_||!support)return nullptr;
        for(std::size_t i=0;i<supports_.size();++i)if(&supports_[i]==support)return bindings_[i].source;
        return nullptr;
    }
}
