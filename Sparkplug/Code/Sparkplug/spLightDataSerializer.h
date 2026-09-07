#pragma once

// Exact original translation-unit path recovered from executable evidence:
// Z:\Sparkplug\Code\Sparkplug\spLightDataSerializer.cpp

#include "spNodeSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spLightData;

    // Native C++ construction/destruction proves spNodeSerializer as the
    // implementation base. Engine RTTI deliberately registers this class
    // directly under spSerializer instead; StaticRTTI preserves that split.
    class spLightDataSerializer : public spNodeSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x33EC2F8E;
        static constexpr spClassID TargetClassID = 0x5E6402DF;

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

        spLightDataSerializer() noexcept = default;
        ~spLightDataSerializer() override;

        spLightDataSerializer(const spLightDataSerializer&) = delete;
        spLightDataSerializer& operator=(const spLightDataSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;

        [[nodiscard]] KnownWritePlan BuildKnownWritePlanForAnalysis(
            const spLightData& light) const;
        [[nodiscard]] std::unique_ptr<spBaseObject> ReadObjectHeaderAndCreateForAnalysis(
            spStream&,spSerializerObjectHeaderForAnalysis* observedHeader=nullptr) const override;
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,spBaseObject&,std::string*) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        [[nodiscard]] bool WritePayloadWithContextForAnalysis(spSerializerManager&,spStream&,
            const spBaseObject&,std::string*) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;
        std::uint32_t defaultWhiteARGBForAnalysis=0xffffffffu;
    private:
        bool WriteSectionsForAnalysis(spSerializerManager*,spStream&,const spBaseObject&,std::string*) const;
    };
}
