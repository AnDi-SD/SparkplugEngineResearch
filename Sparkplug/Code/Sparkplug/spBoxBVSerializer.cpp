#include "spBoxBVSerializer.h"

#include <cmath>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateBoxBVSerializer()
        {
            return std::make_unique<spBoxBVSerializer>();
        }

        const spRTTIRecord BoxBVSerializerRecord{
            spBoxBVSerializer::ClassID,
            spSerializer::ClassID,
            "spBoxBVSerializer",
            &spSerializer::StaticRTTI(),
            &CreateBoxBVSerializer,
            nullptr,
        };

        const bool BoxBVSerializerRegistered =
            spRTTIManager::Instance().Register(BoxBVSerializerRecord);
    }

    bool spBoxBVSerializer::Vector3::operator==(
        const Vector3& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }

    bool spBoxBVSerializer::FieldBinding::operator==(
        const FieldBinding& other) const noexcept
    {
        return field == other.field
            && wireSourceOffset == other.wireSourceOffset
            && decodedMirrorOffset == other.decodedMirrorOffset
            && alwaysWritten == other.alwaysWritten;
    }

    spBoxBVSerializer::~spBoxBVSerializer() = default;

    const spRTTIRecord& spBoxBVSerializer::StaticRTTI() noexcept
    {
        (void)BoxBVSerializerRegistered;
        return BoxBVSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spBoxBVSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spBoxBVSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spBoxBVSerializer::vfunc_18() const noexcept
    {
        return BoxBVSerializerRecord;
    }

    spClassID spBoxBVSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spBoxBVSerializer::FieldBinding>
    spBoxBVSerializer::BuildFieldSchemaForAnalysis()
    {
        return {
            {Field::Position, 0x28, 0x18, false},
            {Field::Size, 0x34, 0, true},
        };
    }

    bool spBoxBVSerializer::IsPositionSuppressedForAnalysis(
        const Vector3& position) noexcept
    {
        return std::fabs(position.x) <= PositionSuppressionEpsilon
            && std::fabs(position.y) <= PositionSuppressionEpsilon
            && std::fabs(position.z) <= PositionSuppressionEpsilon;
    }

    std::vector<spBoxBVSerializer::Field>
    spBoxBVSerializer::BuildWritePlanForAnalysis(const WriteShape& shape)
    {
        std::vector<Field> plan;
        if (!IsPositionSuppressedForAnalysis(shape.position))
        {
            plan.push_back(Field::Position);
        }
        plan.push_back(Field::Size);
        return plan;
    }

    spBoxBVSerializer::DerivedSizeState
    spBoxBVSerializer::DecodeSizeForAnalysis(const Vector3& fullSize) noexcept
    {
        const Vector3 half{
            fullSize.x * 0.5F,
            fullSize.y * 0.5F,
            fullSize.z * 0.5F,
        };
        return {half, std::sqrt(
            half.x * half.x + half.y * half.y + half.z * half.z)};
    }

    bool spBoxBVSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID <= static_cast<std::uint32_t>(Field::Size);
    }
}
