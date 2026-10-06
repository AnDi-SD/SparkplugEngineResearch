#pragma once

// Inferred common path. Original PC identity, embedded evaluator and update
// policy are executable-backed. Portable storage is not the native 0x70 ABI.
#include "spController.h"
#include "spColorFuncEval.h"

namespace sparkplug::reconstruction
{
    class spLight;
    class spLightController final : public spController
    {
    public:
        static constexpr spClassID ClassID = 0x10262533;
        spLightController() = default;
        ~spLightController() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;

        [[nodiscard]] spColorFuncEval& GetColorEvaluatorForAnalysis() noexcept { return color_; }
        [[nodiscard]] const spColorFuncEval& GetColorEvaluatorForAnalysis() const noexcept { return color_; }
        // Native +0x6C is borrowed: this class neither retains nor releases it.
        void SetLightForAnalysis(spLight* light) noexcept { light_ = light; }
        [[nodiscard]] spLight* GetLightForAnalysis() const noexcept { return light_; }
        void ApplyForAnalysis(float delta) override;
        // Host finite-evaluator guard; native direct update has no failure bool.
        [[nodiscard]] bool TryApplyForAnalysis(float delta);

    private:
        spColorFuncEval color_;
        spLight* light_ = nullptr;
    };
}
