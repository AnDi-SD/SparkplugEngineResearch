#include "spScene.h"
#include "spVisibilityManager.h"
#include "spOcclusionVolume.h"
#include "Analysis/PC/spOcclusionSceneMath.h"
#include "Analysis/PC/spVisibilityPointMath.h"
#include <cstring>

namespace sparkplug::reconstruction
{
    namespace
    {
        // Metadata for the analysis object, deliberately not registered as a
        // successful substitute for original45EBC0/45EA10 construction.
        const spRTTIRecord SceneRecord{spScene::ClassID,spNamedObject::ClassID,
            "spScene",&spNamedObject::StaticRTTI(),nullptr,nullptr};
        std::uint32_t KeyBits(float value) noexcept
        {std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
        void Insertion32(std::vector<spScene::OcclusionEntryForAnalysis>& values)
        {
            //45E290 compares current.key<previous.key and rotates the whole
            // insertion range. Equal keys retain their original relative order.
            for(std::size_t i=1;i<values.size();++i)
            {
                const auto current=values[i];auto at=i;
                while(at&&current.orderingKey<values[at-1].orderingKey)
                {values[at]=values[at-1];--at;}
                values[at]=current;
            }
        }
    }
    spScene::spScene(AnalysisConstruction) noexcept{}
    const spRTTIRecord& spScene::StaticRTTI() noexcept{return SceneRecord;}
    const spRTTIRecord& spScene::vfunc_18() const noexcept{return SceneRecord;}
    std::unique_ptr<spBaseObject> spScene::vfunc_10(spCloneManager&) const{return {};}
    spScene::VisibilityOcclusionResultForAnalysis spScene::ProcessVisibilityOcclusionForAnalysis(
        spVisibilityManager& visibility,spVisibilityManager::TraversalSceneForAnalysis& graph,
        const spVisibilityManager::TraversalCameraForAnalysis* passedCamera,
        const OcclusionCameraForAnalysis& passedSortingCamera,const OcclusionSortForAnalysis& largeSort)
    {
        VisibilityOcclusionResultForAnalysis result;
        if(!graph.partitionRoot)return result; // native non-partition Scene branch is outside this operation
        graph.visibilityStamp=visibilityStamp_;
        result.traversal=visibility.ProcessSceneForAnalysis(graph,passedCamera);
        visibilityStamp_=graph.visibilityStamp;
        if(result.traversal!=spVisibilityManager::TraversalStatusForAnalysis::Complete)return result;
        const auto* selected=graph.overrideCamera?graph.overrideCamera:passedCamera;
        // A represented root and successful real traversal require a camera.
        // Keep this host contract explicit rather than synthesize a position.
        if(!selected)return result;
        result.occlusion=ProcessOcclusionForAnalysis(visibility,passedSortingCamera,selected->position,largeSort);
        result.completed=result.occlusion==OcclusionStatusForAnalysis::Complete;
        return result;
    }
    spScene::OcclusionStatusForAnalysis spScene::ProcessOcclusionForAnalysis(
        spVisibilityManager& visibility,const OcclusionCameraForAnalysis& sortingCamera,
        const std::array<float,3>& planeCamera,const OcclusionSortForAnalysis& largeSort)
    {
        using Status=OcclusionStatusForAnalysis;
        using SphereClass=evidence::pc::visibility_math::SphereClass;
        const auto& volumes=visibility.GetVisibleOccludersForAnalysis();
        if(volumes.size()>4096||visibility.GetVisibleForAnalysis().size()>4096)return Status::HostLimit;
        occlusionEntries_.clear(); // original clears logical queue before key construction
        for(auto* volume:volumes)
        {
            if(!volume)return Status::InvalidInput;
            const auto& sphere=volume->GetWorldSphereForAnalysis();float key;
            if(!evidence::pc::occlusion_scene_math::OrderingKey({sphere[0],sphere[1],sphere[2]},
                sortingCamera.view,sortingCamera.orthographic,key))return Status::NumericDomain;
            occlusionEntries_.push_back({volume,key});
        }
        if(occlusionEntries_.size()<=32)Insertion32(occlusionEntries_);
        else
        {
            if(!largeSort.invoke)return Status::MissingOrderingPolicy;
            const auto before=occlusionEntries_;
            if(!largeSort.invoke(largeSort.context,occlusionEntries_.data(),occlusionEntries_.size()))
                return Status::InvalidOrderingPolicy;
            std::vector<bool> used(before.size());
            for(std::size_t i=0;i<occlusionEntries_.size();++i)
            {
                const auto& entry=occlusionEntries_[i];
                if(i&&entry.orderingKey<occlusionEntries_[i-1].orderingKey)return Status::InvalidOrderingPolicy;
                bool found=false;
                for(std::size_t j=0;j<before.size();++j)
                    if(!used[j]&&entry.volume==before[j].volume&&KeyBits(entry.orderingKey)==KeyBits(before[j].orderingKey))
                    {used[j]=found=true;break;}
                if(!found)return Status::InvalidOrderingPolicy;
            }
        }
        for(std::size_t i=0;i<occlusionEntries_.size();++i)
        {
            auto* volume=occlusionEntries_[i].volume;if(!volume)continue;
            const auto built=volume->BuildCameraPlanesForAnalysis(planeCamera);
            if(!built.completed)return Status::NumericDomain;
            //45EE91 ignores originalAL: a failed uninitialized Build may leave
            // an older plane cache which Scene still consumes.
            const auto& planes=volume->GetOcclusionPlanesForAnalysis();
            if(planes.planes.empty())continue; // logical size, NOT activeCount gate
            for(std::size_t j=i+1;j<occlusionEntries_.size();++j)
            {
                auto* later=occlusionEntries_[j].volume;if(!later)continue;
                const auto classification=evidence::pc::visibility_math::Classify(planes,later->GetWorldSphereForAnalysis());
                bool hidden=classification==SphereClass::Inside;
                if(classification==SphereClass::Intersecting)
                {
                    if(!later->IsInitializedForAnalysis()||!later->GetLocalVertexBufferForAnalysis())return Status::MissingGeometry;
                    const auto contained=evidence::pc::visibility_point_math::IsFullyInsidePoints(planes,later->GetPointsForAnalysis());
                    if(!contained.completed)return Status::NumericDomain;
                    hidden=contained.value;
                }
                if(hidden)occlusionEntries_[j].volume=nullptr;
            }
            for(std::size_t j=0;j<visibility.GetVisibleForAnalysis().size();++j)
            {
                const auto* support=visibility.GetVisibleForAnalysis()[j];if(!support)continue;
                if(evidence::pc::visibility_math::IsFullyInside(planes,support->worldSphere))
                    (void)visibility.HideVisibleSupportForAnalysis(j,visibilityStamp_);
            }
        }
        return Status::Complete;
    }
}
