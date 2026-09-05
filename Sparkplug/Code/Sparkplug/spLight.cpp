#include "spLight.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord LightRecord{
            spLight::ClassID,
            spNode::ClassID,
            "spLight",
            &spNode::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool LightRegistered =
            spRTTIManager::Instance().Register(LightRecord);
    }

    spLight::spLight() noexcept = default;

    spLight::~spLight() = default;

    const spRTTIRecord& spLight::StaticRTTI() noexcept
    {
        (void)LightRegistered;
        return LightRecord;
    }

    std::unique_ptr<spBaseObject> spLight::vfunc_10(spCloneManager&) const
    {
        return nullptr;
    }

    bool spLight::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        if (!destination.IsKindOf(ClassID)
            || !spNode::vfunc_14(destination, manager))
        {
            return false;
        }

        auto& lightDestination = static_cast<spLight&>(destination);
        lightDestination.type_ = type_;
        lightDestination.color_ = color_;
        lightDestination.attenuationEnabled_ = attenuationEnabled_;
        lightDestination.opaqueRuntimeFieldBits_ = opaqueRuntimeFieldBits_;
        lightDestination.range_ = range_;
        lightDestination.hotspotAngle_ = hotspotAngle_;
        lightDestination.falloffAngle_ = falloffAngle_;
        lightDestination.projectShadowVolume_ = projectShadowVolume_;
        lightDestination.lightEnabled_ = lightEnabled_;

        // Exact PS2 copy at 0x0016D9F0 deliberately skips +0xE4. A fresh
        // spLightData clone therefore retains the constructor intensity 1.0.
        // The protected PC body cannot independently confirm or refute this.
        return true;
    }

    const spRTTIRecord& spLight::vfunc_18() const noexcept
    {
        return LightRecord;
    }

    spLight::Type spLight::GetTypeForAnalysis() const noexcept
    {
        return type_;
    }

    void spLight::SetTypeForAnalysis(const Type value) noexcept
    {
        type_ = value;
    }

    const spLight::ColorRGBA& spLight::GetColorForAnalysis() const noexcept
    {
        return color_;
    }

    void spLight::SetColorForAnalysis(const ColorRGBA& value) noexcept
    {
        color_ = value;
    }

    bool spLight::ProjectsShadowVolumeForAnalysis() const noexcept
    {
        return projectShadowVolume_;
    }

    void spLight::SetProjectsShadowVolumeForAnalysis(const bool value) noexcept
    {
        projectShadowVolume_ = value;
    }

    bool spLight::UsesAttenuationForAnalysis() const noexcept
    {
        return attenuationEnabled_;
    }

    void spLight::SetUsesAttenuationForAnalysis(const bool value) noexcept
    {
        attenuationEnabled_ = value;
    }

    float spLight::GetIntensityForAnalysis() const noexcept
    {
        return intensity_;
    }

    void spLight::SetIntensityForAnalysis(const float value) noexcept
    {
        intensity_ = value;
    }

    float spLight::GetRangeForAnalysis() const noexcept
    {
        return range_;
    }

    void spLight::SetRangeForAnalysis(const float value) noexcept
    {
        range_ = value;
    }

    float spLight::GetHotspotAngleForAnalysis() const noexcept
    {
        return hotspotAngle_;
    }

    void spLight::SetHotspotAngleForAnalysis(const float value) noexcept
    {
        hotspotAngle_ = value;
    }

    float spLight::GetFalloffAngleForAnalysis() const noexcept
    {
        return falloffAngle_;
    }

    void spLight::SetFalloffAngleForAnalysis(const float value) noexcept
    {
        falloffAngle_ = value;
    }

    bool spLight::IsLightEnabledForAnalysis() const noexcept
    {
        return lightEnabled_;
    }

    void spLight::SetLightEnabledForAnalysis(const bool value) noexcept
    {
        lightEnabled_ = value;
    }

    std::uint32_t spLight::GetOpaqueRuntimeFieldBitsForAnalysis() const noexcept
    {
        return opaqueRuntimeFieldBits_;
    }

    void spLight::SetOpaqueRuntimeFieldBitsForAnalysis(
        const std::uint32_t value) noexcept
    {
        opaqueRuntimeFieldBits_ = value;
    }
}
