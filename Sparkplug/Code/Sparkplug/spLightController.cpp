#include "spLightController.h"
#include "spLight.h"

#include <stdexcept>

namespace sparkplug::reconstruction
{
    spLightController::~spLightController() = default;

    const spRTTIRecord& spLightController::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spController::ClassID, "spLightController",
            &spController::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spLightController>(); }, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }

    const spRTTIRecord& spLightController::vfunc_18() const noexcept { return StaticRTTI(); }

    std::unique_ptr<spBaseObject> spLightController::vfunc_10(spCloneManager& manager) const
    {
        // PC41A9D0 invokes Controller423100: enabled is copied, own evaluator
        // and borrowed Light remain factory defaults. No invented deep copy.
        auto result = std::make_unique<spLightController>();
        manager.RegisterCloneForAnalysis(*this, *result);
        return spController::vfunc_14(*result, manager) ? std::move(result) : nullptr;
    }

    bool spLightController::TryApplyForAnalysis(float delta)
    {
        if (!light_)
            return true;
        std::uint32_t packed = 0;
        if (!color_.EvaluateColorForAnalysis(delta, packed))
            return false;
        // PC424700 uses the original rounded reciprocal, not exact /255.
        constexpr float factor = 0x1.010102p-8F; // binary32 0x3B808081
        spLight::ColorRGBA rgba{};
        constexpr unsigned shifts[] = {16, 8, 0, 24};
        for (unsigned i = 0; i < 4; ++i)
            rgba[i] = static_cast<float>(static_cast<double>((packed >> shifts[i]) & 255U) * factor);
        light_->SetColorForAnalysis(rgba);
        light_->MarkLightDataDirtyForAnalysis();
        return true;
    }

    void spLightController::ApplyForAnalysis(float delta)
    {
        if (!TryApplyForAnalysis(delta))
            throw std::domain_error("LightController evaluator exceeds qualified host finite domain");
    }
}
