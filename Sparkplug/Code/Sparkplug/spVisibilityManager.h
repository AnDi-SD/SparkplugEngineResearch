#pragma once

// Inferred path; PC registration proves the original class name/ID/base.
// PC46D270/46C4D0/46B870 frame and represented partition-tree traversal.
// Original constructor reserve/scratch storage is qualified separately;
// Scene removal and automatic Scene registration remain separate boundaries.
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/PC/spVisibilityMath.h"

namespace sparkplug::reconstruction
{
    class spPartitionNode;
    class spZonePortal;
    class spOcclusionVolume;
    class spVisibilityManager : public spBaseObject
    {
      public:
        static constexpr spClassID ClassID = 0x3D7F4387;
        using PlaneSetForAnalysis = evidence::pc::visibility_math::PlaneSet;
        enum class SupportKindForAnalysis
        {
            Partition,
            Static,
            RenderNode
        };
        // Borrowed test/adapter view, NOT original storage or an original
        // named class. Caller supplies the current native-equivalent fields.
        struct SupportForAnalysis
        {
            SupportKindForAnalysis kind = SupportKindForAnalysis::Static;
            evidence::pc::visibility_math::Sphere worldSphere{};
            std::uint32_t visibilityMark = 0;
            bool enabled = true;
            bool cullBypass = false;
        };

        // Captured camera cache, not a second projection/frustum evaluator.
        // sides are native1E4,204,1F4,214, in that order. The first plane
        // passes through position with forward as normal; near1C4 is unused.
        struct TraversalCameraForAnalysis
        {
            std::array<float,3> position{}, forward{0,0,1};
            evidence::pc::visibility_math::Plane farPlane;
            std::array<evidence::pc::visibility_math::Plane,4> sides{};
        };
        // Borrowed decoded native vectors for an actual reconstructed spatial
        // node. Child topology/zone/Octree queries come from spatial itself.
        // Support values and vectors must remain stable during processing.
        struct TraversalNodeForAnalysis
        {
            spPartitionNode* spatial = nullptr;
            std::uint32_t visibilityMark = 0; // native node+7C
            SupportForAnalysis* payload = nullptr;
            std::vector<SupportForAnalysis*> statics, dynamics;
            bool hasOccluders = false; // legacy marker for an unrepresented native list
            std::vector<spOcclusionVolume*> occluders; // actual volumes, borrowed native+44..48
        };
        enum class TraversalStatusForAnalysis
        {
            Complete,
            InvalidInput,
            MissingNodeView,
            UnsupportedSpatialType,
            UnsupportedPortal,
            UnsupportedOccluder,
            HostTraversalLimit,
        };
        struct TraversalVisitForAnalysis
        {
            spPartitionNode* spatial=nullptr; // borrowed observation, not native storage
            PlaneSetForAnalysis planes; // empty for46B870, which takes no plane argument
            std::array<float,3> incoming{}; // explicit third argument of46C4D0
            bool unclipped=false;
        };
        struct TraversalSceneForAnalysis
        {
            spPartitionNode* partitionRoot = nullptr; // selected system+1D4
            std::vector<TraversalNodeForAnalysis*> nodes;
            const TraversalCameraForAnalysis* overrideCamera = nullptr;
            std::uint32_t visibilityStamp = 0;
            // Optional captured prefix diagnostic. Native manager+10 is the
            // secondary vtable; traversal must preserve this supplied word.
            std::uint32_t opaqueManagerWord10 = 0;
            bool unclippedDebug = false; // resolved DebugManager+21
            // Optional synchronous observations of actual traversal entries.
            // Never replaces a traversal, clipping or success decision.
            std::vector<TraversalVisitForAnalysis>* trace=nullptr;
        };
        // Real frame/zone/root and represented Base/Octree walk. No success
        // callback replaces an internal traversal. Normal portals execute both
        // clip and inclusive-epsilon-band branches; unclipped46B870 omits them.
        // Represented occluders execute sphere/point rejection and stamp/append.
        // Host null/duplicate views, nonfinite camera and visit/depth
        // bounds are explicit safety domains, not native validation claims.
        [[nodiscard]] TraversalStatusForAnalysis ProcessSceneForAnalysis(
            TraversalSceneForAnalysis&, const TraversalCameraForAnalysis*);

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;

        void BeginFrameForAnalysis(std::uint32_t& sceneStamp) noexcept;
        // Invoke in native payload -> static -> dynamic order. No implicit
        // Scene/partition walk is claimed. Accepted records stay borrowed.
        [[nodiscard]] bool TrySubmitForAnalysis(SupportForAnalysis& support,
                                                const PlaneSetForAnalysis& planes,
                                                bool unclippedDebug = false);
        struct OccluderSubmissionForAnalysis {bool completed=false,submitted=false;};
        // PC46C6B8/46B9BC, direct-owned geometry stays with the volume. This
        // method neither initializes geometry nor replaces a failed query with
        // success. A clipped intersecting sphere requires represented world VB.
        [[nodiscard]] OccluderSubmissionForAnalysis TrySubmitOccluderForAnalysis(
            spOcclusionVolume&,const PlaneSetForAnalysis&,bool unclippedDebug=false);
        void SetSphereCullingForAnalysis(bool value) noexcept
        {
            sphereCulling_ = value;
        }
        void SetFrameStampForAnalysis(std::uint32_t value) noexcept
        {
            frameStamp_ = value;
        }
        [[nodiscard]] std::uint32_t GetFrameStampForAnalysis() const noexcept
        {
            return frameStamp_;
        }
        [[nodiscard]] const std::vector<SupportForAnalysis*>& GetVisibleForAnalysis() const noexcept
        {
            return visible_;
        }
        [[nodiscard]] const auto& GetVisibleOccludersForAnalysis() const noexcept{return visibleOccluders_;}
        // Borrowed output mutation used by the separate Scene CPU phase.
        // Native45EFB9/45EFC1 writes mark=Scene40-1 and retains a null slot.
        [[nodiscard]] bool HideVisibleSupportForAnalysis(std::size_t index,std::uint32_t sceneStamp) noexcept
        {
            if(index>=visible_.size()||!visible_[index])return false;
            visible_[index]->visibilityMark=sceneStamp-1u;visible_[index]=nullptr;return true;
        }

      private:
        struct PortalStepForAnalysis
        {
            bool traverse=false;
            PlaneSetForAnalysis planes;
            std::array<float,3> incoming{};
        };
        [[nodiscard]] static TraversalStatusForAnalysis PreparePortalForAnalysis(
            const spZonePortal&,const TraversalCameraForAnalysis&,const std::array<float,3>&,
            const PlaneSetForAnalysis&,PortalStepForAnalysis&);
        std::uint32_t frameStamp_ = 0;
        bool sphereCulling_ = true;
        std::vector<SupportForAnalysis*> visible_;
        std::vector<spOcclusionVolume*> visibleOccluders_;
    };
} // namespace sparkplug::reconstruction
