#include "spUVControllerSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateUVControllerSerializer()
        {
            return std::make_unique<spUVControllerSerializer>();
        }

        const spRTTIRecord UVControllerSerializerRecord{
            spUVControllerSerializer::ClassID,
            spSerializer::ClassID,
            "spUVControllerSerializer",
            &spSerializer::StaticRTTI(),
            &CreateUVControllerSerializer,
            nullptr,
        };

        const bool UVControllerSerializerRegistered =
            spRTTIManager::Instance().Register(UVControllerSerializerRecord);
    }

    bool spUVControllerSerializer::EvaluatorBinding::operator==(
        const EvaluatorBinding& other) const noexcept
    {
        return role == other.role
            && transformOffset == other.transformOffset
            && targetOffset == other.targetOffset;
    }

    bool spUVControllerSerializer::VectorBinding::operator==(
        const VectorBinding& other) const noexcept
    {
        return role == other.role
            && transformOffset == other.transformOffset
            && targetOffset == other.targetOffset;
    }

    spUVControllerSerializer::~spUVControllerSerializer() = default;

    const spRTTIRecord& spUVControllerSerializer::StaticRTTI() noexcept
    {
        (void)UVControllerSerializerRegistered;
        return UVControllerSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spUVControllerSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spUVControllerSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spUVControllerSerializer::vfunc_18() const noexcept
    {
        return UVControllerSerializerRecord;
    }

    spClassID spUVControllerSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spUVControllerSerializer::Field>
    spUVControllerSerializer::BuildWritePlanForAnalysis()
    {
        return {Field::UVController};
    }

    spUVControllerSerializer::EvaluatorPlan
    spUVControllerSerializer::BuildEvaluatorPlanForAnalysis() noexcept
    {
        return {{
            {EvaluatorRole::TranslationX, 0x10, 0x5C},
            {EvaluatorRole::TranslationY, 0x48, 0x94},
            {EvaluatorRole::TranslationZ, 0x80, 0xCC},
            {EvaluatorRole::ScaleX, 0xB8, 0x104},
            {EvaluatorRole::ScaleY, 0xF0, 0x13C},
            {EvaluatorRole::ScaleZ, 0x128, 0x174},
            {EvaluatorRole::Rotation, 0x178, 0x1C4},
        }};
    }

    spUVControllerSerializer::VectorPlan
    spUVControllerSerializer::BuildVectorPlanForAnalysis() noexcept
    {
        return {{
            {VectorRole::UVPivot, 0x160, 0x1AC},
            {VectorRole::RotationAxis, 0x16C, 0x1B8},
        }};
    }

    bool spUVControllerSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID == static_cast<std::uint32_t>(Field::UVController);
    }
}
