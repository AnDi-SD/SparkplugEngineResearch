#include "spTransFunctionEvalSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTransFunctionEvalSerializer()
        {
            return std::make_unique<spTransFunctionEvalSerializer>();
        }

        const spRTTIRecord TransFunctionEvalSerializerRecord{
            spTransFunctionEvalSerializer::ClassID,
            spSerializer::ClassID,
            "spTransFunctionEvalSerializer",
            &spSerializer::StaticRTTI(),
            &CreateTransFunctionEvalSerializer,
            nullptr,
        };

        const bool TransFunctionEvalSerializerRegistered =
            spRTTIManager::Instance().Register(TransFunctionEvalSerializerRecord);
    }

    bool spTransFunctionEvalSerializer::EvaluatorBinding::operator==(
        const EvaluatorBinding& other) const noexcept
    {
        return role == other.role && targetOffset == other.targetOffset;
    }

    bool spTransFunctionEvalSerializer::VectorBinding::operator==(
        const VectorBinding& other) const noexcept
    {
        return role == other.role && targetOffset == other.targetOffset;
    }

    spTransFunctionEvalSerializer::~spTransFunctionEvalSerializer() = default;

    const spRTTIRecord& spTransFunctionEvalSerializer::StaticRTTI() noexcept
    {
        (void)TransFunctionEvalSerializerRegistered;
        return TransFunctionEvalSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spTransFunctionEvalSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTransFunctionEvalSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spTransFunctionEvalSerializer::vfunc_18() const noexcept
    {
        return TransFunctionEvalSerializerRecord;
    }

    spClassID
    spTransFunctionEvalSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spTransFunctionEvalSerializer::Field>
    spTransFunctionEvalSerializer::BuildWritePlanForAnalysis()
    {
        return {Field::TransformFunctions};
    }

    spTransFunctionEvalSerializer::EvaluatorPlan
    spTransFunctionEvalSerializer::BuildEvaluatorPlanForAnalysis() noexcept
    {
        return {{
            {EvaluatorRole::TranslationX, 0x10},
            {EvaluatorRole::TranslationY, 0x48},
            {EvaluatorRole::TranslationZ, 0x80},
            {EvaluatorRole::ScaleX, 0xB8},
            {EvaluatorRole::ScaleY, 0xF0},
            {EvaluatorRole::ScaleZ, 0x128},
            {EvaluatorRole::Rotation, 0x178},
        }};
    }

    spTransFunctionEvalSerializer::VectorPlan
    spTransFunctionEvalSerializer::BuildVectorPlanForAnalysis() noexcept
    {
        return {{
            {VectorRole::UVPivot, 0x160},
            {VectorRole::RotationAxis, 0x16C},
        }};
    }

    bool spTransFunctionEvalSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID == static_cast<std::uint32_t>(Field::TransformFunctions);
    }
}
