#include "spSphereBVSerializer.h"

#include <cmath>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateSphereBVSerializer()
        {
            return std::make_unique<spSphereBVSerializer>();
        }

        const spRTTIRecord SphereBVSerializerRecord{
            spSphereBVSerializer::ClassID,
            spSerializer::ClassID,
            "spSphereBVSerializer",
            &spSerializer::StaticRTTI(),
            &CreateSphereBVSerializer,
            nullptr,
        };

        const bool SphereBVSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(SphereBVSerializerRecord);
    }

    bool spSphereBVSerializer::FieldBinding::operator==(
        const FieldBinding& other) const noexcept
    {
        return field == other.field
            && wireSourceOffset == other.wireSourceOffset
            && decodedMirrorOffset == other.decodedMirrorOffset
            && alwaysWritten == other.alwaysWritten;
    }

    spSphereBVSerializer::~spSphereBVSerializer() = default;

    const spRTTIRecord& spSphereBVSerializer::StaticRTTI() noexcept
    {
        (void)SphereBVSerializerRegistered;
        return SphereBVSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spSphereBVSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spSphereBVSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spSphereBVSerializer::vfunc_18() const noexcept
    {
        return SphereBVSerializerRecord;
    }

    spClassID spSphereBVSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spSphereBVSerializer::FieldBinding>
    spSphereBVSerializer::BuildFieldSchemaForAnalysis()
    {
        return {
            {Field::Position, 0x28, 0x18, false},
            {Field::Radius, 0x34, 0x24, true},
        };
    }

    bool spSphereBVSerializer::IsPositionSuppressedForAnalysis(
        const Vector3& position) noexcept
    {
        return std::fabs(position.x) <= PositionSuppressionEpsilon
            && std::fabs(position.y) <= PositionSuppressionEpsilon
            && std::fabs(position.z) <= PositionSuppressionEpsilon;
    }

    std::vector<spSphereBVSerializer::Field>
    spSphereBVSerializer::BuildWritePlanForAnalysis(const WriteShape& shape)
    {
        std::vector<Field> plan;
        if (!IsPositionSuppressedForAnalysis(shape.position))
        {
            plan.push_back(Field::Position);
        }
        plan.push_back(Field::Radius);
        return plan;
    }

    bool spSphereBVSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID <= static_cast<std::uint32_t>(Field::Radius);
    }
}
