#include "spLightControllerSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateLightControllerSerializer()
        {
            return std::make_unique<spLightControllerSerializer>();
        }

        const spRTTIRecord LightControllerSerializerRecord{
            spLightControllerSerializer::ClassID,
            spSerializer::ClassID,
            "spLightControllerSerializer",
            &spSerializer::StaticRTTI(),
            &CreateLightControllerSerializer,
            nullptr,
        };

        const bool LightControllerSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(LightControllerSerializerRecord);
    }

    bool spLightControllerSerializer::FieldBinding::operator==(
        const FieldBinding& other) const noexcept
    {
        return field == other.field
            && wireKind == other.wireKind
            && defaultRule == other.defaultRule
            && targetOffset == other.targetOffset;
    }

    spLightControllerSerializer::~spLightControllerSerializer() = default;

    const spRTTIRecord& spLightControllerSerializer::StaticRTTI() noexcept
    {
        (void)LightControllerSerializerRegistered;
        return LightControllerSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spLightControllerSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spLightControllerSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spLightControllerSerializer::vfunc_18() const noexcept
    {
        return LightControllerSerializerRecord;
    }

    spClassID
    spLightControllerSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    spLightControllerSerializer::FieldSchema
    spLightControllerSerializer::BuildFieldSchemaForAnalysis() noexcept
    {
        return {{
            {Field::Enabled, WireKind::Boolean, DefaultRule::Zero, 0x10},
            {Field::Color1, WireKind::ColorARGB, DefaultRule::PlatformColor, 0x2C},
            {Field::Color2, WireKind::ColorARGB, DefaultRule::PlatformColor, 0x30},
            {Field::Type, WireKind::UInt32, DefaultRule::Zero, 0x68},
            {Field::Frequency, WireKind::Float32, DefaultRule::One, 0x48},
            {Field::Amplitude, WireKind::Float32, DefaultRule::One, 0x50},
            {Field::Offset, WireKind::Float32, DefaultRule::Zero, 0x58},
            {Field::Pitch, WireKind::Float32, DefaultRule::Zero, 0x5C},
            {Field::Light, WireKind::Relationship, DefaultRule::Always, 0x6C},
        }};
    }

    std::vector<spLightControllerSerializer::Field>
    spLightControllerSerializer::BuildWritePlanForAnalysis(
        const WriteShape& shape,
        const std::uint32_t platformDefaultColorARGB)
    {
        std::vector<Field> plan;
        if (shape.enabled)
        {
            plan.push_back(Field::Enabled);
        }
        if (shape.color1ARGB != platformDefaultColorARGB)
        {
            plan.push_back(Field::Color1);
        }
        if (shape.color2ARGB != platformDefaultColorARGB)
        {
            plan.push_back(Field::Color2);
        }
        if (shape.type != 0)
        {
            plan.push_back(Field::Type);
        }
        if (shape.frequency != 1.0F)
        {
            plan.push_back(Field::Frequency);
        }
        if (shape.amplitude != 1.0F)
        {
            plan.push_back(Field::Amplitude);
        }
        if (shape.offset != 0.0F)
        {
            plan.push_back(Field::Offset);
        }
        if (shape.pitch != 0.0F)
        {
            plan.push_back(Field::Pitch);
        }
        plan.push_back(Field::Light);
        return plan;
    }

    spLightControllerSerializer::FrequencyState
    spLightControllerSerializer::DecodeFrequencyForAnalysis(
        const float wireFrequency) noexcept
    {
        return {wireFrequency, 1.0F / wireFrequency};
    }

    bool spLightControllerSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID <= static_cast<std::uint32_t>(Field::Light);
    }
}
