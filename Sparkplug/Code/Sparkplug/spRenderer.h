#pragma once

// Inferred common header path. Both shipped executables register spRenderer
// directly below spCrossPlatform, but no original header path survives.

#include "../SparkBase/spBaseObject.h"
#include "Analysis/PC/spRendererQueueMath.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spCamera;
    class spRenderNode;
    class spRenderable;
    class spDXMaterial;
    // Analytical names for the platform-interface operations whose behavior
    // is now proven from their native callers and backend endpoints. These
    // are not claimed to be the original C++ method names.
    enum class spRendererPlatformForAnalysis : std::uint8_t
    {
        PC,
        PS2,
    };

    enum class spRendererPlatformOperationForAnalysis : std::uint8_t
    {
        BindRenderTarget,
        BindCubeRenderTarget,
        BeginScene,
        EndScene,
        Clear,
        SubmitMesh,
        Configure2D,
        SetProjectionMatrix,
        SetViewMatrix,
        SetWorldMatrix,
        SetViewport,
        SetTextureTransform,
        SetFog,
        SetUVTransform3x3, // PC24, PS2 material boundary23; unlike PC4x4 slot23
    };

    // Common renderer owner. The native class is abstract (null RTTI factory)
    // and exposes a separate 29-slot platform-render interface at +0x18.
    // That interface is intentionally not named here until its original type
    // or method names are recovered.
    class spRenderer : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x2D9C0296;
        static constexpr std::size_t PlatformInterfaceSlotCount = 29;
        static constexpr std::size_t RenderStateCacheCount = 12;

        ~spRenderer() override;

        spRenderer(const spRenderer&) = delete;
        spRenderer& operator=(const spRenderer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spRenderer* GetInstance() noexcept;

        // PC-only producer from the original common constructor13B890E..13B89B5.
        // Returns the owned C9C0 material graph without constructing a renderer
        // or device. Its specular power remains unknown until explicitly set.
        [[nodiscard]] static std::unique_ptr<spDXMaterial> CreatePCDefaultMaterialForAnalysis();

        // PC and PS2 swap the first two render-target operations. The other
        // confirmed camera/frame operations retain their logical ordinals.
        [[nodiscard]] static std::size_t
            GetPlatformInterfaceSlotForAnalysis(
                spRendererPlatformForAnalysis platform,
                spRendererPlatformOperationForAnalysis operation) noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // PC 0x00454940 and PS2 0x00179E60 both invalidate twelve render
        // states. Texture-state capacity is selected by the platform leaf.
        [[nodiscard]] bool InvalidateStateCachesForAnalysis() noexcept;
        [[nodiscard]] std::size_t GetRenderStateCacheCountForAnalysis()
            const noexcept;
        [[nodiscard]] std::size_t GetTextureStateCacheCountForAnalysis()
            const noexcept;
        [[nodiscard]] std::uint32_t GetRenderStateCacheForAnalysis(
            std::size_t index) const noexcept;
        [[nodiscard]] std::uint32_t GetTextureStateCacheForAnalysis(
            std::size_t index) const noexcept;

        struct AlphaCameraInputForAnalysis final
        {
            spCamera* identity=nullptr;
            // Explicit original camera+CC cache and distinct byte231 branch;
            // neither is inferred from the serialized camera Is2D setting.
            evidence::pc::renderer_queue_math::Matrix4 view{};
            bool depthOnly=false;
        };
        struct AlphaEntryForAnalysis final
        {
            spCamera* camera=nullptr;
            spRenderNode* support=nullptr;
            spRenderable* renderable=nullptr;
            evidence::pc::renderer_queue_math::AlphaKey key;
        };
        struct AlphaQueueForAnalysis final
        {
            static constexpr std::size_t Capacity=2048;
            std::array<AlphaEntryForAnalysis,Capacity> entries{};
            std::uint32_t count=0,priorityBase=0;
            bool enabled=true,flushing=false,sortTransparent=true;
            bool dispatching=false; // explicit host reentry guard, not native44
        };
        // PC454C30: disabled45 succeeds before any object reads. Otherwise
        // sphere/matrices/metric precede capacity check. All records BORROW.
        [[nodiscard]] static bool EnqueueAlphaForAnalysis(AlphaQueueForAnalysis&,
            spRenderable*,spRenderNode*,const AlphaCameraInputForAnalysis*,std::uint32_t priority) noexcept;
        struct AlphaDispatchForAnalysis final
        {
            using Compare=int (*)(const AlphaEntryForAnalysis&,const AlphaEntryForAnalysis&) noexcept;
            // Actual external CRT qsort boundary. Native comparator never
            // returns zero; no host std::sort or unproved tie policy is hidden.
            void (*sort)(void*,AlphaEntryForAnalysis*,std::size_t,Compare)=nullptr;
            bool (*prepare)(void*,spRenderNode&)=nullptr;
            bool (*render)(void*,spRenderable&,spCamera*,spRenderNode*)=nullptr;
            void* context=nullptr;
        };
        // PC454850: sort even an empty queue, then flag44=1; adjacent support
        // reuse, ignored virtual results, live count reread, stale record bytes.
        [[nodiscard]] static bool FlushAlphaForAnalysis(AlphaQueueForAnalysis&,const AlphaDispatchForAnalysis&);

        // PC456310/456910: separate normal record20 and nine record8 vectors.
        // Pointers are borrowed. Portable records are NOT the native32 ABI.
        struct GeneralEntryForAnalysis final
        {
            spRenderable* renderable=nullptr;
            void* support=nullptr; // adjusted support interface, not always RenderNode
            spCamera* camera=nullptr;
            std::uint32_t materialKey=0;
            std::uintptr_t meshIdentity=0;
        };
        struct BucketEntryForAnalysis final
        {
            spRenderable* renderable=nullptr;
            void* support=nullptr;
        };
        struct GeneralQueueStateForAnalysis final
        {
            // Explicit host bound; no native vector limit is inferred.
            static constexpr std::size_t MaximumEntries=65536;
            std::vector<GeneralEntryForAnalysis> general;
            std::array<std::vector<BucketEntryForAnalysis>,9> buckets;
            bool enabled=false; // explicit caller state, not native startup evidence
            bool nineBucketMode=false; // original75F8E8; startup value remains unknown
            bool dispatching=false; // host enqueue/flush reentry guard
        };
        struct GeneralQueueInputForAnalysis final
        {
            GeneralEntryForAnalysis general;
            // Original support slot14 and raw renderable14, supplied explicitly.
            // No PC bucket classifier is invented. General branch ignores both.
            bool supportEnabled=true;
            std::uint32_t mode=0;
        };
        // materialKey is the observed material4C->18->10->virtual1C result;
        // meshIdentity is Model/derived58 or0. Unknown owner lookup stays an
        // explicit input boundary rather than a guessed material-pointer key.
        // Disabled C050 returns false before input reads. Bucket disabled
        // support returns true before mode/camera/key/mesh are consumed.
        [[nodiscard]] static bool EnqueueGeneralForAnalysis(GeneralQueueStateForAnalysis&,
            const GeneralQueueInputForAnalysis*);
        struct GeneralDispatchForAnalysis final
        {
            using Less=bool (*)(const GeneralEntryForAnalysis&,const GeneralEntryForAnalysis&) noexcept;
            // Native456090 is a compiler sort, NOT imported CRT qsort. Caller
            // supplies its chosen sort policy; native tie permutation unproved.
            void (*sort)(void*,GeneralEntryForAnalysis*,std::size_t,Less)=nullptr;
            bool (*prepare)(void*,void* support)=nullptr;
            bool (*render)(void*,spRenderable*,spCamera*,void* support)=nullptr;
            // Seven selected PC virtual phases are all5B7A00 no-ops. Optional
            // observer receives native byte slot20/24/28/3C/40/2C/30 and camera;
            // this is not a replacement bucket renderer or bucket traversal.
            void (*pcBucketPhase)(void*,std::uint32_t,spCamera*)=nullptr;
            void* context=nullptr;
        };
        // flags correspond in order to globals740240..740244. Slots3C/40
        // always run. Both branches clear only GENERAL logical contents;
        // bucket contents/capacity and enabled C050 survive the flush.
        [[nodiscard]] static bool FlushGeneralForAnalysis(GeneralQueueStateForAnalysis&,
            spCamera*,const std::array<bool,5>&,const GeneralDispatchForAnalysis&);
        // PC455FE0 logical clear versus456B10->456AC0 storage release.
        static void ClearGeneralForAnalysis(GeneralQueueStateForAnalysis&) noexcept;
        static void ReleaseGeneralForAnalysis(GeneralQueueStateForAnalysis&) noexcept;
        // Explicit caller/Scene phase operation; flush does not drain buckets.
        static void ClearBucketQueuesForAnalysis(GeneralQueueStateForAnalysis&) noexcept;

    protected:
        explicit spRenderer(std::size_t textureStateCacheCount);
        // Shared word loop of PC454940; storage belongs to the caller.
        static void InvalidateCacheWordsForAnalysis(std::uint32_t*,std::size_t) noexcept;

    private:
        static spRenderer* instance_;
        std::vector<std::uint32_t> renderStateCache_;
        std::vector<std::uint32_t> textureStateCache_;
    };
}
