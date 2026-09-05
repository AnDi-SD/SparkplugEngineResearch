#include "spColorFuncEvalSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateColorFuncEvalSerializer()
        {
            return std::make_unique<spColorFuncEvalSerializer>();
        }

        const spRTTIRecord ColorFuncEvalSerializerRecord{
            spColorFuncEvalSerializer::ClassID,
            spSerializer::ClassID,
            "spColorFuncEvalSerializer",
            &spSerializer::StaticRTTI(),
            &CreateColorFuncEvalSerializer,
            nullptr,
        };

        const bool ColorFuncEvalSerializerRegistered =
            spRTTIManager::Instance().Register(ColorFuncEvalSerializerRecord);
    }

    bool spColorFuncEvalSerializer::FieldBinding::operator==(
        const FieldBinding& other) const noexcept
    {
        return field == other.field
            && wireKind == other.wireKind
            && defaultRule == other.defaultRule
            && targetOffset == other.targetOffset;
    }

    spColorFuncEvalSerializer::~spColorFuncEvalSerializer() = default;

    const spRTTIRecord& spColorFuncEvalSerializer::StaticRTTI() noexcept
    {
        (void)ColorFuncEvalSerializerRegistered;
        return ColorFuncEvalSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spColorFuncEvalSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spColorFuncEvalSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spColorFuncEvalSerializer::vfunc_18() const noexcept
    {
        return ColorFuncEvalSerializerRecord;
    }

    spClassID spColorFuncEvalSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    spColorFuncEvalSerializer::FieldSchema
    spColorFuncEvalSerializer::BuildFieldSchemaForAnalysis() noexcept
    {
        return {{
            {Field::Color1, WireKind::ColorARGB, DefaultRule::PlatformColor, 0x10},
            {Field::Color2, WireKind::ColorARGB, DefaultRule::PlatformColor, 0x14},
            {Field::FunctionType, WireKind::UInt32, DefaultRule::Zero, 0x4C},
            {Field::Frequency, WireKind::Float32, DefaultRule::One, 0x2C},
            {Field::Amplitude, WireKind::Float32, DefaultRule::One, 0x34},
            {Field::XOffset, WireKind::Float32, DefaultRule::Zero, 0x38},
            {Field::YOffset, WireKind::Float32, DefaultRule::Zero, 0x3C},
            {Field::Pitch, WireKind::Float32, DefaultRule::Zero, 0x40},
        }};
    }

    std::vector<spColorFuncEvalSerializer::Field>
    spColorFuncEvalSerializer::BuildWritePlanForAnalysis(
        const WriteShape& shape,
        const std::uint32_t platformDefaultColorARGB)
    {
        std::vector<Field> plan;
        if (shape.color1ARGB != platformDefaultColorARGB)
        {
            plan.push_back(Field::Color1);
        }
        if (shape.color2ARGB != platformDefaultColorARGB)
        {
            plan.push_back(Field::Color2);
        }
        if (shape.functionType != 0)
        {
            plan.push_back(Field::FunctionType);
        }
        if (shape.frequency != 1.0F)
        {
            plan.push_back(Field::Frequency);
        }
        if (shape.amplitude != 1.0F)
        {
            plan.push_back(Field::Amplitude);
        }
        if (shape.xOffset != 0.0F)
        {
            plan.push_back(Field::XOffset);
        }
        if (shape.yOffset != 0.0F)
        {
            plan.push_back(Field::YOffset);
        }
        if (shape.pitch != 0.0F)
        {
            plan.push_back(Field::Pitch);
        }
        return plan;
    }

    spColorFuncEvalSerializer::FrequencyState
    spColorFuncEvalSerializer::DecodeFrequencyForAnalysis(
        const float wireFrequency) noexcept
    {
        return {wireFrequency, 1.0F / wireFrequency};
    }

    bool spColorFuncEvalSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID <= static_cast<std::uint32_t>(Field::Pitch);
    }
}
