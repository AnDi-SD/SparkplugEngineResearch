#pragma once
// Inferred portable declaration. PC48D6F0/49BCE0/49C8D0 are executable-backed;
// the lost source ABI and the particle simulation are not reconstructed here.
#include "spRenderable.h"

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
        // Only the non-looping initial state of native48C340/48BE50 is supported.
        // Looping initialization calls48D1C0 and already emits live particles.
        bool PrepareNonLoopingForAnalysis() noexcept;
        bool SampleEmissionRegionForAnalysis(sparkplug::evidence::pc::ParticleRandomForAnalysis&,
            std::uint32_t count,std::vector<Vector3>& output) const;
        const std::array<std::uint32_t,5>& GetPoolStateForAnalysis() const noexcept {return poolState_;}
    private:
        ParametersForAnalysis parameters_;
        std::weak_ptr<spRenderNode> renderNode_; // native +5C is borrowed; avoids graph cycle
        std::array<std::uint32_t,5> poolState_{};
    };
}
