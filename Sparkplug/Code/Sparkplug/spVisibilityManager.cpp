#include "spVisibilityManager.h"
#include "spPartitionNode.h"
#include "spOctreeNode.h"
#include "spZone.h"
#include "spZonePortal.h"
#include "spOcclusionVolume.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateVisibilityManager()
        {
            return std::make_unique<spVisibilityManager>();
        }
        const spRTTIRecord VisibilityRecord{
            spVisibilityManager::ClassID, spBaseObject::ClassID,    "spVisibilityManager",
            &spBaseObject::StaticRTTI(),  &CreateVisibilityManager, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(VisibilityRecord);
    } // namespace
    const spRTTIRecord& spVisibilityManager::StaticRTTI() noexcept
    {
        (void)Registered;
        return VisibilityRecord;
    }
    const spRTTIRecord& spVisibilityManager::vfunc_18() const noexcept
    {
        return VisibilityRecord;
    }
    std::unique_ptr<spBaseObject> spVisibilityManager::vfunc_10(spCloneManager& manager) const
    {
        // Static original46D220: fresh factory + inherited Base copy. Native
        // factory/teardown is separately qualified, not a full clone proof.
        auto clone = std::make_unique<spVisibilityManager>();
        manager.RegisterClone(*this, *clone);
        return spBaseObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    void spVisibilityManager::BeginFrameForAnalysis(std::uint32_t& sceneStamp) noexcept
    {
        ++frameStamp_; // unsigned wrap, including the observed zero-stamp frame
        sceneStamp = frameStamp_;
        visible_.clear();
        visibleOccluders_.clear();
    }
    bool spVisibilityManager::TrySubmitForAnalysis(SupportForAnalysis& support,
                                                   const PlaneSetForAnalysis& planes,
                                                   const bool unclippedDebug)
    {
        const bool dynamic = support.kind == SupportKindForAnalysis::RenderNode;
        if ((!unclippedDebug && dynamic && !support.enabled) ||
            support.visibilityMark == frameStamp_)
            return false;
        if (!unclippedDebug && sphereCulling_ && !(dynamic && support.cullBypass) &&
            evidence::pc::visibility_math::IsOutside(planes, support.worldSphere))
            return false;
        visible_.push_back(&support);
        support.visibilityMark = frameStamp_;
        return true;
    }

    spVisibilityManager::TraversalStatusForAnalysis spVisibilityManager::ProcessSceneForAnalysis(
        TraversalSceneForAnalysis& scene, const TraversalCameraForAnalysis* camera)
    {
        using Status = TraversalStatusForAnalysis;
        using Math = evidence::pc::visibility_math::Plane;
        if(!scene.partitionRoot)
        {
            BeginFrameForAnalysis(scene.visibilityStamp);
            if(scene.trace)scene.trace->clear();
            return Status::Complete; // no camera, graph or word10 access on this native branch
        }
        // Preflight the represented graph, before publishing a partial frame.
        // This is a host restriction; native unsafe/unsupported paths are not
        // converted into successful empty visibility.
        if(scene.nodes.size()>4096) return Status::HostTraversalLimit;
        for(std::size_t i=0;i<scene.nodes.size();++i)
        {
            const auto* view=scene.nodes[i];
            if(!view||!view->spatial) return Status::InvalidInput;
            if(view->hasOccluders) return Status::UnsupportedOccluder;
            if(view->occluders.size()>4096)return Status::HostTraversalLimit;
            for(const auto* volume:view->occluders)
            {
                if(!volume)return Status::InvalidInput;
                for(float value:volume->GetWorldSphereForAnalysis())if(!std::isfinite(value))return Status::InvalidInput;
            }
            if(view->spatial->GetPortalsForAnalysis().size()>4096)return Status::HostTraversalLimit;
            for(const auto& portal:view->spatial->GetPortalsForAnalysis())
                if(!portal)return Status::InvalidInput;
            if(!view->spatial->IsExactly(spPartitionNode::ClassID)&&
                !dynamic_cast<const spOctreeNode*>(view->spatial)) return Status::UnsupportedSpatialType;
            for(std::size_t j=0;j<i;++j)
                if(scene.nodes[j]->spatial==view->spatial) return Status::InvalidInput;
            for(const auto* support:view->statics)if(!support) return Status::InvalidInput;
            for(const auto* support:view->dynamics)if(!support) return Status::InvalidInput;
            if(view->payload&&view->payload->kind!=SupportKindForAnalysis::Partition)return Status::InvalidInput;
            for(const auto* support:view->statics)if(support->kind!=SupportKindForAnalysis::Static)return Status::InvalidInput;
            for(const auto* support:view->dynamics)if(support->kind!=SupportKindForAnalysis::RenderNode)return Status::InvalidInput;
        }
        const auto* selected=scene.overrideCamera?scene.overrideCamera:camera;
        if(scene.partitionRoot)
        {
            if(!selected) return Status::InvalidInput;
            for(float value:selected->position)if(!std::isfinite(value))return Status::InvalidInput;
            for(float value:selected->forward)if(!std::isfinite(value))return Status::InvalidInput;
            const auto finite=[](const Math& plane)
            {for(float value:plane.equation)if(!std::isfinite(value))return false;return true;};
            if(!finite(selected->farPlane))return Status::InvalidInput;
            for(const auto& plane:selected->sides)if(!finite(plane))return Status::InvalidInput;
        }
        const auto find=[&](spPartitionNode* node)->TraversalNodeForAnalysis*
        {
            for(auto* view:scene.nodes)if(view->spatial==node)return view;
            return nullptr;
        };
        // Bound the selected FindLeaf path before invoking its recursive
        // virtual implementation. The later visit guard cannot protect this
        // earlier query. Stable owned child topology is a caller precondition.
        std::vector<spPartitionNode*> queryPath;
        auto* query=scene.partitionRoot;
        while(query)
        {
            if(queryPath.size()>=128||std::find(queryPath.begin(),queryPath.end(),query)!=queryPath.end())
                return Status::HostTraversalLimit;
            if(!find(query))return Status::MissingNodeView;
            queryPath.push_back(query);
            const auto* octree=dynamic_cast<const spOctreeNode*>(query);
            if(!octree||octree->HasZoneForAnalysis())break;
            if(!octree->HasGeometryForAnalysis())return Status::InvalidInput;
            query=query->GetChildForAnalysis(spOctreeNode::OctantForAnalysis(selected->position,octree->GetPivotForAnalysis()));
        }
        if(!query)return Status::InvalidInput;
        BeginFrameForAnalysis(scene.visibilityStamp);
        if(scene.trace)scene.trace->clear();
        auto* leaf=scene.partitionRoot->FindLeafForAnalysis(selected->position,true);
        if(!leaf||!leaf->GetZoneForAnalysis())return Status::InvalidInput; // native dereferences
        const auto& roots=leaf->GetZoneForAnalysis()->GetRootsForAnalysis();
        PlaneSetForAnalysis initial;
        const auto& p=selected->position;const auto& n=selected->forward;
        initial.planes.push_back({{n[0],n[1],n[2],static_cast<float>(
            (double(p[1])*n[1]+double(p[2])*n[2])+double(p[0])*n[0])},true});
        auto far=selected->farPlane;far.enabled=true;initial.planes.push_back(far);
        for(auto side:selected->sides){side.enabled=true;initial.planes.push_back(side);}
        initial.activeCount=6;
        std::size_t visits=0;
        std::vector<spPartitionNode*> active;
        Status result=Status::Complete;
        std::function<bool(spPartitionNode*,const PlaneSetForAnalysis&,bool,const std::array<float,3>&)> visit;
        visit=[&](spPartitionNode* spatial,const PlaneSetForAnalysis& planes,bool unclipped,
            const std::array<float,3>& incoming)
        {
            if(!spatial){result=Status::InvalidInput;return false;}
            if(++visits>4096||active.size()>=128)
            {result=Status::HostTraversalLimit;return false;}
            auto* node=find(spatial);
            if(!node){result=Status::MissingNodeView;return false;}
            active.push_back(spatial);
            struct Pop{std::vector<spPartitionNode*>& values;~Pop(){values.pop_back();}}pop{active};
            if(scene.trace)scene.trace->push_back({spatial,unclipped?PlaneSetForAnalysis{}:planes,
                unclipped?std::array<float,3>{}:incoming,unclipped});
            node->visibilityMark=frameStamp_; // not a skip gate at entry
            if(node->payload)(void)TrySubmitForAnalysis(*node->payload,planes,unclipped);
            for(auto* support:node->statics)(void)TrySubmitForAnalysis(*support,planes,unclipped);
            for(auto* support:node->dynamics)(void)TrySubmitForAnalysis(*support,planes,unclipped);
            for(auto* volume:node->occluders)
                if(!TrySubmitOccluderForAnalysis(*volume,planes,unclipped).completed)
                {result=Status::UnsupportedOccluder;return false;}
            auto* octree=dynamic_cast<const spOctreeNode*>(spatial);
            if(octree&&!octree->HasGeometryForAnalysis()){result=Status::InvalidInput;return false;}
            if(unclipped)
            {
                if(!octree)return true;
                const auto mask=octree->PointMaskForAnalysis(selected->position);
                unsigned first=0;while(first<8&&!(mask&(1u<<first)))++first;
                if(first==8){result=Status::InvalidInput;return false;}
                const auto order=spOctreeNode::TraversalOrderForAnalysis(first);
                for(unsigned i=0;i<8;++i)
                    if(!visit(spatial->GetChildForAnalysis((order>>(3*i))&7),planes,true,{}))return false;
                return true;
            }
            if(octree)
            {
                const auto packed=octree->VisibleChildrenForAnalysis(planes,selected->position);
                const auto count=packed&15u;
                for(unsigned i=0;i<count;++i)
                {
                    const auto child=(packed>>(4+3*i))&7;
                    auto reduced=planes;
                    octree->ReducePlanesForChildForAnalysis(child,reduced);
                    // Native drops the new stack slot when active count is equal;
                    // zero planes uses46B870 and omits Enabled/portal processing.
                    const auto& next=reduced.activeCount==planes.activeCount?planes:reduced;
                    if(!visit(spatial->GetChildForAnalysis(child),next,!reduced.activeCount,incoming))return false;
                }
            }
            for(const auto& portal:spatial->GetPortalsForAnalysis())
            {
                if(!portal->IsOpen()||portal->GetVisibilityMarkForAnalysis()==frameStamp_)continue;
                // Root marks are not a recursion gate. Native stamps each
                // portal BEFORE geometry; directed A->B->A cycles end here.
                portal->SetVisibilityMarkForAnalysis(frameStamp_);
                PortalStepForAnalysis step;
                result=PreparePortalForAnalysis(*portal,*selected,incoming,planes,step);
                if(result!=Status::Complete)return false;
                if(!step.traverse)continue;
                const auto* destination=portal->GetDestinationZone();
                if(!destination){result=Status::InvalidInput;return false;}
                for(auto* root:destination->GetRootsForAnalysis())
                    if(!visit(root,step.planes,false,step.incoming))return false;
            }
            return true;
        };
        for(auto* root:roots)
        {
            // Native Debug21 loops Zone count but repeats selected system root.
            if(!visit(scene.unclippedDebug?scene.partitionRoot:root,initial,scene.unclippedDebug,{}))return result;
        }
        //46D4D9 changes ESI from manager to the first plane-set record;
        //46D508 clears that record's activeCount10, not manager+10 (vtable).
        return Status::Complete;
    }
} // namespace sparkplug::reconstruction
