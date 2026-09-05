#include "spOBBBVSerializer.h"

#include <cmath>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateOBBBVSerializer()
        {
            return std::make_unique<spOBBBVSerializer>();
        }

        const spRTTIRecord OBBBVSerializerRecord{
            spOBBBVSerializer::ClassID,
            spSerializer::ClassID,
            "spOBBBVSerializer",
            &spSerializer::StaticRTTI(),
            &CreateOBBBVSerializer,
            nullptr,
        };

        const bool OBBBVSerializerRegistered =
            spRTTIManager::Instance().Register(OBBBVSerializerRecord);

        constexpr spOBBBVSerializer::Matrix3 IdentityMatrix{
            1.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 1.0F,
        };
    }

    bool spOBBBVSerializer::Vector3::operator==(
        const Vector3& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }

    bool spOBBBVSerializer::FieldBinding::operator==(
        const FieldBinding& other) const noexcept
    {
        return field == other.field
            && wireSourceOffset == other.wireSourceOffset
            && decodedTargetOffset == other.decodedTargetOffset
            && encoding == other.encoding
            && alwaysWritten == other.alwaysWritten;
    }

    spOBBBVSerializer::~spOBBBVSerializer() = default;

    const spRTTIRecord& spOBBBVSerializer::StaticRTTI() noexcept
    {
        (void)OBBBVSerializerRegistered;
        return OBBBVSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spOBBBVSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spOBBBVSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spOBBBVSerializer::vfunc_18() const noexcept
    {
        return OBBBVSerializerRecord;
    }

    spClassID spOBBBVSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spOBBBVSerializer::FieldBinding>
    spOBBBVSerializer::BuildFieldSchemaForAnalysis()
    {
        return {
            {Field::Position, 0x4C, 0x18, WireEncoding::Vector3, false},
            {Field::Size, 0x58, 0x58, WireEncoding::Vector3, true},
            {Field::Rotation, 0x28, 0x28,
                WireEncoding::QuaternionFromMatrix3, false},
        };
    }

    bool spOBBBVSerializer::IsPositionSuppressedForAnalysis(
        const Vector3& position) noexcept
    {
        return std::fabs(position.x) <= SuppressionEpsilon
            && std::fabs(position.y) <= SuppressionEpsilon
            && std::fabs(position.z) <= SuppressionEpsilon;
    }

    bool spOBBBVSerializer::IsRotationSuppressedForAnalysis(
        const Matrix3& rotation) noexcept
    {
        for (std::size_t index = 0; index < rotation.size(); ++index)
        {
            if (std::fabs(rotation[index] - IdentityMatrix[index])
                > SuppressionEpsilon)
            {
                return false;
            }
        }
        return true;
    }

    std::vector<spOBBBVSerializer::Field>
    spOBBBVSerializer::BuildWritePlanForAnalysis(const WriteShape& shape)
    {
        std::vector<Field> plan;
        if (!IsPositionSuppressedForAnalysis(shape.position))
        {
            plan.push_back(Field::Position);
        }
        plan.push_back(Field::Size);
        if (!IsRotationSuppressedForAnalysis(shape.rotation))
        {
            plan.push_back(Field::Rotation);
        }
        return plan;
    }

    spOBBBVSerializer::DerivedSizeState
    spOBBBVSerializer::DecodeSizeForAnalysis(const Vector3& fullSize) noexcept
    {
        const Vector3 half{
            fullSize.x * 0.5F,
            fullSize.y * 0.5F,
            fullSize.z * 0.5F,
        };
        return {half, std::sqrt(
            half.x * half.x + half.y * half.y + half.z * half.z)};
    }

    bool spOBBBVSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID <= static_cast<std::uint32_t>(Field::Rotation);
    }
}
