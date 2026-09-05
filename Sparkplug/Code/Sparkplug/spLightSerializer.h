#pragma once

// Inferred declaration/translation-unit path. The class name survives in
// both shipped executables, but no original spLightSerializer source path has
// been recovered.

#include "spNodeSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spLight;

    class spLightSerializer : public spNodeSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x06165309;
        static constexpr spClassID TargetClassID = 0x72444900;

        enum class Field : std::uint32_t
        {
            Type = 0,
            ProjectShadowVolume = 1,
            Color = 2,
            Attenuation = 3,
            Intensity = 4,
            Range = 5,
            HotspotAngle = 6,
            FalloffAngle = 7,
            Enabled = 8,
        };

        struct KnownWritePlan final
        {
            std::vector<spNodeSerializer::Field> nodeFields;
            std::vector<Field> lightFields;

            [[nodiscard]] bool operator==(const KnownWritePlan& other) const noexcept;
        };

        spLightSerializer() noexcept = default;
        ~spLightSerializer() override;

        spLightSerializer(const spLightSerializer&) = delete;
        spLightSerializer& operator=(const spLightSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;

        [[nodiscard]] KnownWritePlan BuildKnownWritePlanForAnalysis(
            const spLight& light) const;
    };
}
