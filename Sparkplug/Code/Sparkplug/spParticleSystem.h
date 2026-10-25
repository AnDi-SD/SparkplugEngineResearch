#pragma once
// Inferred portable declaration. PC48D6F0/49BCE0/49C8D0 are executable-backed;
// Source APIs and host-owned ring storage are not the lost native ABI.
#include "spRenderable.h"
#include "Analysis/PC/spBallisticCpuDraw.h"

namespace sparkplug::evidence::pc {struct ParticleRandomForAnalysis;}

namespace sparkplug::reconstruction
{
    class spRenderNode;
    class spParticleSystem final : public spRenderable
    {
    public:
        static constexpr spClassID ClassID=0x5AFA1A4F;
        using Vector3=std::array<float,3>;
        struct ParametersForAnalysis
        {
            std::array<Vector3,2> acceleration{{{0,0,1},{0,0,1}}};
            Vector3 direction{};
            std::array<float,2> velocity{0,1},angle{0,180},scale{1,1};
            std::array<std::uint32_t,2> colors{0xffffffff,0};
            std::array<float,2> times{-1,1};
            std::array<std::uint8_t,3> flags{1,1,0};
            float rate=100;
            BoundingSphere sphere{};
            // Native tags: point1, box2, sphere3, plane4, disk5, cylinder6, cone7.
            std::uint32_t regionType=0;
            std::vector<float> region;
        };
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&,spCloneManager&) const override;
        ParametersForAnalysis& Parameters() noexcept {return parameters_;}
        const ParametersForAnalysis& Parameters() const noexcept {return parameters_;}
        const BoundingSphere& GetBoundingSphereForAnalysis() const noexcept override {return parameters_.sphere;}
        void SetRenderNodeForAnalysis(const std::shared_ptr<spRenderNode>& node) noexcept {renderNode_=node;}
        std::shared_ptr<spRenderNode> GetRenderNodeForAnalysis() const noexcept {return renderNode_.lock();}
        // Compatibility count inspection only, without an owned CPU pool.
        // Actual readers use InitializeForAnalysis for both loop modes.
        bool PrepareNonLoopingForAnalysis() noexcept;
        // CPU pool/reset/loop producer: PC48C340/48BE50/48D1C0/48C400.
        // Host owns storage; old renderer/device initialization is excluded.
        // Finite 2..1024 capacity and current explicit RenderNode world input.
        bool InitializeForAnalysis(sparkplug::evidence::pc::ParticleRandomForAnalysis* random=nullptr,
            std::string* error=nullptr);
        using ParticleRecordForAnalysis=std::array<float,8>; // position, velocity, birth, lifetime
        using PoolLinkForAnalysis=std::array<std::uint32_t,3>; // physical record, previous, next
        const std::vector<ParticleRecordForAnalysis>& GetRecordsForAnalysis() const noexcept {return records_;}
        const std::vector<PoolLinkForAnalysis>& GetLinksForAnalysis() const noexcept {return links_;}
        const std::vector<std::uint8_t>& GetRecordWrittenForAnalysis() const noexcept {return recordWritten_;}
        std::uint32_t GetFirstForAnalysis() const noexcept {return first_;}
        std::uint32_t GetBoundaryForAnalysis() const noexcept {return boundary_;}
        bool IsInitializedForAnalysis() const noexcept {return initialized_;}
        bool SampleEmissionRegionForAnalysis(sparkplug::evidence::pc::ParticleRandomForAnalysis&,
            std::uint32_t count,std::vector<Vector3>& output) const;
        const std::array<std::uint32_t,5>& GetPoolStateForAnalysis() const noexcept {return poolState_;}
        struct ProductionForAnalysis final
        {
            std::uint32_t limitedRequest=0,created=0;
            // Native48C400 returns limitedRequest even when the final128-batch
            // remainder bug creates fewer records. Update ignores this return.
        };
        struct ModifierForAnalysis final
        {
            void (*apply)(void*,float delta)=nullptr; // original virtual24(delta)
            void* context=nullptr; // borrowed, stable for the synchronous update
            // Callback may edit records, not parameters/clock/links/owner life.
        };
        struct FrameOptionsForAnalysis final
        {
            float integrationRate=30; // native+B0, separate from emission rate
            bool emissionEnabled=false; // explicit native+58 input, not guessed startup
            std::vector<ModifierForAnalysis> modifiers;
        };
        struct FrameStateForAnalysis final {float delta=0,clock=0,accumulator=0;};
        void SetFrameStateForAnalysis(FrameStateForAnalysis) noexcept;
        [[nodiscard]] FrameStateForAnalysis GetFrameStateForAnalysis() const noexcept;
        // Physical record index; explicit fixture/adapter setter, not an
        // asserted original API. Callbacks may change record values, not links.
        bool SetRecordForAnalysis(std::size_t,const ParticleRecordForAnalysis&) noexcept;
        [[nodiscard]] bool ProduceParticlesForAnalysis(std::uint32_t requested,float time,
            ProductionForAnalysis&,sparkplug::evidence::pc::ParticleRandomForAnalysis* random=nullptr,
            std::string* error=nullptr);
        //48D450/48D210 orchestration: physical ring retirement, actual producer,
        // original requested-count debit, virtual modifier callbacks, integration.
        // Host finite/count/reentry guards do not claim native fault behavior.
        // Runtime refusal after mutation preserves completed native-order work;
        // no rollback of records, callbacks, clocks or shared randomness is promised.
        [[nodiscard]] bool UpdateForAnalysis(float delta,const FrameOptionsForAnalysis&,
            sparkplug::evidence::pc::ParticleRandomForAnalysis* random=nullptr,std::string* error=nullptr);
        struct CpuDrawInputForAnalysis final
        {
            std::array<float,9> cameraOrientation{1,0,0,0,1,0,0,0,1};
            Vector3 nodePosition{},nodeScale{1,1,1};
            std::array<float,2> runtimeScales{1,1}; // native100/104, explicit runtime values
            std::uint32_t vertexCapacity=0,indexCapacity=0;
        };
        struct CpuDrawDispatchForAnalysis final
        {
            bool (*setWorld)(void*,const std::array<float,16>&)=nullptr;
            // Raw clock/birth/lifetime preserve the existing x87 color model's
            // inputs; passing only a rounded double age would lose precision.
            evidence::pc::BallisticCpuColorProviderForAnalysis color=nullptr;
            // Concrete packed24-byte vertices / uint16 indices; receiver must
            // consume/copy synchronously. Native per-batch false is ignored.
            bool (*uploadAndDraw)(void*,const evidence::pc::BallisticCpuPackedVertexForAnalysis*,
                std::uint32_t vertexCount,const std::uint16_t*,std::uint32_t indexCount,
                std::uint32_t primitiveKind,std::uint32_t primitiveCount)=nullptr;
            void* context=nullptr;
        };
        struct CpuDrawStateForAnalysis final
        {
            std::uint8_t effectField1C=0; // reset only after successful SetWorld
            std::uint32_t submittedBatches=0,rejectedSubmissionReplies=0; // host observations
        };
        [[nodiscard]] bool DrawCpuForAnalysis(const CpuDrawInputForAnalysis&,
            const CpuDrawDispatchForAnalysis&,CpuDrawStateForAnalysis&,std::string* error=nullptr) const;
    private:
        bool CapacityForAnalysis(std::uint32_t& count) const noexcept;
        bool ProduceParticlesImplForAnalysis(std::uint32_t,float,ProductionForAnalysis&,
            sparkplug::evidence::pc::ParticleRandomForAnalysis*,std::string*);
        ParametersForAnalysis parameters_;
        std::weak_ptr<spRenderNode> renderNode_; // native +5C is borrowed; avoids graph cycle
        std::array<std::uint32_t,5> poolState_{};
        std::vector<ParticleRecordForAnalysis> records_;
        std::vector<PoolLinkForAnalysis> links_;
        // Original allocator leaves seven words unwritten until emission.
        // Host zeros those words and exposes validity, never allocator poison.
        std::vector<std::uint8_t> recordWritten_;
        std::uint32_t first_=0,boundary_=0;
        bool initialized_=false;
        bool updating_=false;
    };
}
