#pragma once

// Original identity/lifecycle and PC incoming-PRS operation. Analytical
// state/API names and portable storage are not a native ABI declaration.
#include "spTransformEval.h"
#include "spFunctionEval.h"

namespace sparkplug::reconstruction
{
    class spTransformConstEval final : public spTransformEval
    {
    public:
        static constexpr spClassID ClassID = 0x68BCC047;
        struct StateForAnalysis final
        {
            Vector3 velocity{};
            Vector3 axis{0, 0, 1};
            float angle = 0;
            spFunctionEval::StateForAnalysis scaleFunction;
            // Native +0x60 is the embedded FunctionEval's type (+0x34),
            // not a separate boolean. Type zero disables scale altogether.
            std::uint8_t positionEnabled = 0, rotationEnabled = 0;
        };

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] StateForAnalysis GetStateForAnalysis() const noexcept;
        void SetStateForAnalysis(const StateForAnalysis&) noexcept;
        // Native601B20 updates caller-owned PRS in place, preserving disabled
        // channel bytes but always replacing all three validity flags.
        // Host preflight rejects unsupported nonfinite
        // math before mutation. No such guard is claimed for the native code.
        [[nodiscard]] bool EvaluateInPlaceForAnalysis(float time, SampleForAnalysis&);
        // Host convenience supplies zero position, identity rotation and unit
        // scale as incoming values; the native operation itself has no seed.
        [[nodiscard]] SampleForAnalysis EvaluateForAnalysis(float time) override;

    private:
        StateForAnalysis state_;
        spFunctionEval scaleFunction_;
    };
}
