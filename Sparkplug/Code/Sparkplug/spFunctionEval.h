#pragma once
// Original RTTI identity; original header/API names remain unknown. PC scalar
// semantics reconstructed from478680 and serializer47EE00/47F220, not PS2.
#include "spEvaluator.h"
#include "Analysis/PC/spParticleSampling.h"
#include <cstdint>
namespace sparkplug::reconstruction
{
    class spFunctionEval final : public spEvaluator
    {
    public:
        static constexpr spClassID ClassID=0x9450E590;
        struct StateForAnalysis final
        {
            float time=0,frequency=1,reciprocal=1,amplitude=1;
            float xOffset=0,yOffset=0,pitch=0,clampLimit=0;
            bool clampEnabled=false;
            std::uint32_t functionType=0; // raw native IDs, NOT recovered enum names
        };
        // No native RTTI/class name is claimed for this analytical wrapper.
        // FunctionEval and Particle use the SAME original global413270/4132B0,
        // state755658 and index73FE8C. Keep one algorithm and the existing
        // SharedRandom singleton; explicit analytical states remain supported.
        using RandomStateForAnalysis=evidence::pc::ParticleRandomForAnalysis;
        [[nodiscard]] static RandomStateForAnalysis& SharedRandomForAnalysis();
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] const StateForAnalysis& GetStateForAnalysis() const noexcept{return state_;}
        // Raw serialized state stays representable, including IEEE payloads;
        // finite evaluation is a separate guarded contract.
        void SetStateForAnalysis(const StateForAnalysis& state) noexcept{state_=state;}
        void SetFrequencyForAnalysis(float value) noexcept{state_.frequency=value;state_.reciprocal=1.0F/value;}
        [[nodiscard]] bool EvaluateForAnalysis(float delta,float& output,RandomStateForAnalysis* random=nullptr);
        // Native scalar return stays in x87 until its caller stores it.
        // Rotation multiplies by2pi BEFORE that first float32 store.
        [[nodiscard]] bool EvaluateExtendedForAnalysis(float delta,double& output,RandomStateForAnalysis* random=nullptr);
    private:
        StateForAnalysis state_;
    };
}
