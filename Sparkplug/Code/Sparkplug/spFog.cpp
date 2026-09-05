#include "spFog.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateFog()
        {
            return std::make_unique<spFog>();
        }

        const spRTTIRecord FogRecord{
            spFog::ClassID,
            spBaseObject::ClassID,
            "spFog",
            &spBaseObject::StaticRTTI(),
            &CreateFog,
            nullptr,
        };

        const bool FogRegistered =
            spRTTIManager::Instance().Register(FogRecord);
    }

    spFog::spFog() noexcept = default;

    spFog::~spFog() = default;

    const spRTTIRecord& spFog::StaticRTTI() noexcept
    {
        (void)FogRegistered;
        return FogRecord;
    }

    std::unique_ptr<spBaseObject> spFog::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spFog>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    const spRTTIRecord& spFog::vfunc_18() const noexcept
    {
        return FogRecord;
    }

    spFog::Type spFog::GetTypeForAnalysis() const noexcept
    {
        return type_;
    }

    void spFog::SetTypeForAnalysis(const Type value) noexcept
    {
        type_ = value;
    }

    std::uint32_t spFog::GetColorARGBForAnalysis() const noexcept
    {
        return colorARGB_;
    }

    void spFog::SetColorARGBForAnalysis(const std::uint32_t value) noexcept
    {
        colorARGB_ = value;
    }

    float spFog::GetStartForAnalysis() const noexcept
    {
        return start_;
    }

    void spFog::SetStartForAnalysis(const float value) noexcept
    {
        start_ = value;
    }

    float spFog::GetEndForAnalysis() const noexcept
    {
        return end_;
    }

    void spFog::SetEndForAnalysis(const float value) noexcept
    {
        end_ = value;
    }

    float spFog::GetDensityForAnalysis() const noexcept
    {
        return density_;
    }

    void spFog::SetDensityForAnalysis(const float value) noexcept
    {
        density_ = value;
    }
}
