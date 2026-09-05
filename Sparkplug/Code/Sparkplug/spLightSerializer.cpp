#include "spLightSerializer.h"

#include "spLight.h"

#include <cmath>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateLightSerializer()
        {
            return std::make_unique<spLightSerializer>();
        }

        const spRTTIRecord LightSerializerRecord{
            spLightSerializer::ClassID,
            spNodeSerializer::ClassID,
            "spLightSerializer",
            &spNodeSerializer::StaticRTTI(),
            &CreateLightSerializer,
            nullptr,
        };

        const bool LightSerializerRegistered =
            spRTTIManager::Instance().Register(LightSerializerRecord);

        bool IsDefaultLightColor(const spLight::ColorRGBA& color) noexcept
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

    bool spLightSerializer::KnownWritePlan::operator==(
        const KnownWritePlan& other) const noexcept
    {
        return nodeFields == other.nodeFields
            && lightFields == other.lightFields;
    }

    spLightSerializer::~spLightSerializer() = default;

    const spRTTIRecord& spLightSerializer::StaticRTTI() noexcept
    {
        (void)LightSerializerRegistered;
        return LightSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spLightSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spLightSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spLightSerializer::vfunc_18() const noexcept
    {
        return LightSerializerRecord;
    }

    spClassID spLightSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spLight::ClassID;
    }

    spLightSerializer::KnownWritePlan
    spLightSerializer::BuildKnownWritePlanForAnalysis(const spLight& light) const
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
        if (!IsDefaultLightColor(light.GetColorForAnalysis()))
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
