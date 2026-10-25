#pragma once

// Inferred path. Partial PC Scene45EC70 CPU occlusion phase. Physical Named
// prefix is established by45EA30 CALL413060, inherited413120 and own fields
// starting+14. This is independent of the matching registered base class ID.
#include "Code/SparkBase/spBaseObject.h"
#include "spVisibilityManager.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spOcclusionVolume;
    class spVisibilityManager;
    class spScene : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID=0x6F927C11;
        struct AnalysisConstruction {};
        // Does not execute45EA10, register with SceneManager, create a system
        // root or own the four native subsystems. Native factory/clone remain
        // unavailable; there is no successful RTTI construction registration.
        explicit spScene(AnalysisConstruction) noexcept;
        ~spScene() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;

        struct OcclusionEntryForAnalysis
        {
            spOcclusionVolume* volume=nullptr; // borrowed; null holes retained
            float orderingKey=0;
        };
        struct OcclusionSortForAnalysis
        {
            void* context=nullptr;
            bool (*invoke)(void*,OcclusionEntryForAnalysis*,std::size_t)=nullptr;
        };
        struct OcclusionCameraForAnalysis
        {
            std::array<float,16> view{}; // actual cached camera+CC, column-major
            bool orthographic=false; // actual camera+231, not serialized2D flag
        };
        enum class OcclusionStatusForAnalysis
        {
            Complete,InvalidInput,NumericDomain,MissingGeometry,MissingOrderingPolicy,
            InvalidOrderingPolicy,HostLimit
        };
        // Scene40 is published by Visibility before this phase. Keeping it as
        // an explicit field preserves the Scene-stamp-minus-one support write.
        void SetVisibilityStampForAnalysis(std::uint32_t value) noexcept{visibilityStamp_=value;}
        [[nodiscard]] auto GetVisibilityStampForAnalysis() const noexcept{return visibilityStamp_;}
        [[nodiscard]] const auto& GetOcclusionEntriesForAnalysis() const noexcept{return occlusionEntries_;}
        // CPU phase45ED05..45EFE3 only: build keys, sort, build actual volume
        // planes, null hidden later-volume/support slots and roll back support
        // marks. The passed sorting camera may differ from the override camera
        // position used for plane building, exactly as the original callers do.
        // <=32 entries use the original45E290 stable insertion result. Larger
        // cohorts require an explicit policy; its native equivalence is NOT
        // inferred. The host verifies sorted keys and preserved whole records.
        [[nodiscard]] OcclusionStatusForAnalysis ProcessOcclusionForAnalysis(
            spVisibilityManager&,const OcclusionCameraForAnalysis& sortingCamera,
            const std::array<float,3>& planeCamera,
            const OcclusionSortForAnalysis& largeSort);
        struct VisibilityOcclusionResultForAnalysis
        {
            bool completed=false;
            spVisibilityManager::TraversalStatusForAnalysis traversal=
                spVisibilityManager::TraversalStatusForAnalysis::InvalidInput;
            OcclusionStatusForAnalysis occlusion=OcclusionStatusForAnalysis::InvalidInput;
        };
        // Actual partition-path CPU composition45ECCB..45EFE3. The represented
        // partition root is required: original non-partition rendering follows
        // another branch. Traversal executes its concrete body and publishes
        // Scene40 before occlusion. Sorting uses passed-camera cache; plane
        // construction uses override camera position when present. No callback
        // replaces traversal, geometry, clipping or visibility decisions.
        [[nodiscard]] VisibilityOcclusionResultForAnalysis ProcessVisibilityOcclusionForAnalysis(
            spVisibilityManager&,spVisibilityManager::TraversalSceneForAnalysis&,
            const spVisibilityManager::TraversalCameraForAnalysis* passedCamera,
            const OcclusionCameraForAnalysis& passedSortingCamera,
            const OcclusionSortForAnalysis& largeSort);
    private:
        std::uint32_t visibilityStamp_=0; // nativeScene+40
        std::vector<OcclusionEntryForAnalysis> occlusionEntries_; // logical nativeScene+44
    };
}
