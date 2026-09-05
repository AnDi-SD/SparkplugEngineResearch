#include "spFunctionEvalSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateFunctionEvalSerializer()
        {
            return std::make_unique<spFunctionEvalSerializer>();
        }

        const spRTTIRecord FunctionEvalSerializerRecord{
            spFunctionEvalSerializer::ClassID,
            spSerializer::ClassID,
            "spFunctionEvalSerializer",
            &spSerializer::StaticRTTI(),
            &CreateFunctionEvalSerializer,
            nullptr,
        };

        const bool FunctionEvalSerializerRegistered =
            spRTTIManager::Instance().Register(FunctionEvalSerializerRecord);
    }

    bool spFunctionEvalSerializer::FieldBinding::operator==(
        const FieldBinding& other) const noexcept
    {
        return field == other.field
            && wireKind == other.wireKind
            && defaultRule == other.defaultRule
            && targetOffset == other.targetOffset;
    }

    spFunctionEvalSerializer::~spFunctionEvalSerializer() = default;

    const spRTTIRecord& spFunctionEvalSerializer::StaticRTTI() noexcept
    {
        (void)FunctionEvalSerializerRegistered;
        return FunctionEvalSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spFunctionEvalSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spFunctionEvalSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spFunctionEvalSerializer::vfunc_18() const noexcept
    {
        return FunctionEvalSerializerRecord;
    }

    spClassID spFunctionEvalSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    spFunctionEvalSerializer::FieldSchema
    spFunctionEvalSerializer::BuildFieldSchemaForAnalysis() noexcept
    {
        return {{
            {Field::FunctionType, WireKind::UInt32, DefaultRule::Zero, 0x34},
            {Field::Frequency, WireKind::Float32, DefaultRule::One, 0x14},
            {Field::Amplitude, WireKind::Float32, DefaultRule::One, 0x1C},
            {Field::XOffset, WireKind::Float32, DefaultRule::Zero, 0x20},
            {Field::YOffset, WireKind::Float32, DefaultRule::Zero, 0x24},
            {Field::Pitch, WireKind::Float32, DefaultRule::Zero, 0x28},
        }};
    }

    std::vector<spFunctionEvalSerializer::Field>
    spFunctionEvalSerializer::BuildWritePlanForAnalysis(const WriteShape& shape)
    {
        std::vector<Field> plan;
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

    spFunctionEvalSerializer::FrequencyState
    spFunctionEvalSerializer::DecodeFrequencyForAnalysis(
        const float wireFrequency) noexcept
    {
        return {wireFrequency, 1.0F / wireFrequency};
    }

    bool spFunctionEvalSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID <= static_cast<std::uint32_t>(Field::Pitch);
    }
}
