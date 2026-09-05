#include "spLightDataSerializer.h"

#include "spLightData.h"

#include <cmath>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateLightDataSerializer()
        {
            return std::make_unique<spLightDataSerializer>();
        }

        const spRTTIRecord LightDataSerializerRecord{
            spLightDataSerializer::ClassID,
            spSerializer::ClassID,
            "spLightDataSerializer",
            &spSerializer::StaticRTTI(),
            &CreateLightDataSerializer,
            nullptr,
        };

        const bool LightDataSerializerRegistered =
            spRTTIManager::Instance().Register(LightDataSerializerRecord);

        bool IsDefaultColor(const spLight::ColorRGBA& color) noexcept
        {
            for (const float component : color)
            {
                if (std::fabs(component - 1.0F)
                    > spNodeSerializer::DefaultComparisonTolerance)
                {
                    return false;
                }
            }
            return true;
        }
    }

    bool spLightDataSerializer::KnownWritePlan::operator==(
        const KnownWritePlan& other) const noexcept
    {
        return nodeFields == other.nodeFields
            && lightFields == other.lightFields;
    }

    spLightDataSerializer::~spLightDataSerializer() = default;

    const spRTTIRecord& spLightDataSerializer::StaticRTTI() noexcept
    {
        (void)LightDataSerializerRegistered;
        return LightDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spLightDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spLightDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spLightDataSerializer::vfunc_18() const noexcept
    {
        return LightDataSerializerRecord;
    }

    spClassID spLightDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spLightData::ClassID;
    }

    spLightDataSerializer::KnownWritePlan
    spLightDataSerializer::BuildKnownWritePlanForAnalysis(
        const spLightData& light) const
    {
        KnownWritePlan plan;
        plan.nodeFields =
            spNodeSerializer::BuildKnownWritePlanForAnalysis(light);

        if (light.GetTypeForAnalysis() != spLight::Type::Directional)
        {
            plan.lightFields.push_back(Field::Type);
        }
        if (light.ProjectsShadowVolumeForAnalysis())
        {
            plan.lightFields.push_back(Field::ProjectShadowVolume);
        }
        if (!IsDefaultColor(light.GetColorForAnalysis()))
        {
            plan.lightFields.push_back(Field::Color);
        }
        if (light.UsesAttenuationForAnalysis())
        {
            plan.lightFields.push_back(Field::Attenuation);
        }
        if (light.GetIntensityForAnalysis() != spLight::DefaultIntensity)
        {
            plan.lightFields.push_back(Field::Intensity);
        }
        if (light.GetRangeForAnalysis() != spLight::DefaultRange)
        {
            plan.lightFields.push_back(Field::Range);
        }
        if (light.GetHotspotAngleForAnalysis() != 0.0F)
        {
            plan.lightFields.push_back(Field::HotspotAngle);
        }
        if (light.GetFalloffAngleForAnalysis() != 0.0F)
        {
            plan.lightFields.push_back(Field::FalloffAngle);
        }
        if (light.IsLightEnabledForAnalysis())
        {
            plan.lightFields.push_back(Field::Enabled);
        }

        return plan;
    }
}
