#include "spMatColorControllerSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMatColorControllerSerializer()
        {
            return std::make_unique<spMatColorControllerSerializer>();
        }

        const spRTTIRecord MatColorControllerSerializerRecord{
            spMatColorControllerSerializer::ClassID,
            spSerializer::ClassID,
            "spMatColorControllerSerializer",
            &spSerializer::StaticRTTI(),
            &CreateMatColorControllerSerializer,
            nullptr,
        };

        const bool MatColorControllerSerializerRegistered =
            spRTTIManager::Instance().Register(MatColorControllerSerializerRecord);
    }

    bool spMatColorControllerSerializer::EvaluatorBinding::operator==(
        const EvaluatorBinding& other) const noexcept
    {
        return role == other.role
            && kind == other.kind
            && targetOffset == other.targetOffset;
    }

    spMatColorControllerSerializer::~spMatColorControllerSerializer() = default;

    const spRTTIRecord& spMatColorControllerSerializer::StaticRTTI() noexcept
    {
        (void)MatColorControllerSerializerRegistered;
        return MatColorControllerSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spMatColorControllerSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMatColorControllerSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spMatColorControllerSerializer::vfunc_18() const noexcept
    {
        return MatColorControllerSerializerRecord;
    }

    spClassID
    spMatColorControllerSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spMatColorControllerSerializer::Field>
    spMatColorControllerSerializer::BuildWritePlanForAnalysis()
    {
        return {Field::MaterialColorController};
    }

    spMatColorControllerSerializer::EvaluatorPlan
    spMatColorControllerSerializer::BuildEvaluatorPlanForAnalysis() noexcept
    {
        return {{
            {EvaluatorRole::Ambient, EvaluatorKind::ColorFunctional, 0x68},
            {EvaluatorRole::Diffuse, EvaluatorKind::ColorFunctional, 0xB8},
            {EvaluatorRole::Specular, EvaluatorKind::ColorFunctional, 0x108},
            {EvaluatorRole::Emissive, EvaluatorKind::ColorFunctional, 0x158},
            {EvaluatorRole::Alpha, EvaluatorKind::Functional, 0x1A8},
        }};
    }

    bool spMatColorControllerSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID
            == static_cast<std::uint32_t>(Field::MaterialColorController);
    }
}
