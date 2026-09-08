#pragma once

// The native class and field diagnostics survive in both executables, but an
// exact original translation-unit path has not yet been recovered.

#include "spSerializer.h"

#include <array>
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spOBBBVSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x68EA2ED1;
        static constexpr spClassID TargetClassID = 0x4DA04889;
        static constexpr float SuppressionEpsilon = 0.001F;

        enum class Field : std::uint32_t
        {
            Position = 0,
            Size = 1,
            Rotation = 2,
        };

        enum class WireEncoding : std::uint8_t
        {
            Vector3,
            QuaternionFromMatrix3,
        };

        struct Vector3 final
        {
            float x = 0.0F;
            float y = 0.0F;
            float z = 0.0F;

            [[nodiscard]] bool operator==(const Vector3& other) const noexcept;
        };

        using Matrix3 = std::array<float, 9>;

        struct FieldBinding final
        {
            Field field;
            std::uint32_t wireSourceOffset;
            std::uint32_t decodedTargetOffset;
            WireEncoding encoding;
            bool alwaysWritten;

            [[nodiscard]] bool operator==(
                const FieldBinding& other) const noexcept;
        };

        struct WriteShape final
        {
            Vector3 position;
            Vector3 size;
            Matrix3 rotation{
                1.0F, 0.0F, 0.0F,
                0.0F, 1.0F, 0.0F,
                0.0F, 0.0F, 1.0F,
            };
        };

        struct DerivedSizeState final
        {
            Vector3 halfExtents;
            float boundingSphereRadius;
        };

        spOBBBVSerializer() noexcept = default;
        ~spOBBBVSerializer() override;

        spOBBBVSerializer(const spOBBBVSerializer&) = delete;
        spOBBBVSerializer& operator=(const spOBBBVSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept;

        [[nodiscard]] static std::vector<FieldBinding>
            BuildFieldSchemaForAnalysis();
        [[nodiscard]] static std::vector<Field> BuildWritePlanForAnalysis(
            const WriteShape& shape);
        [[nodiscard]] static bool IsPositionSuppressedForAnalysis(
            const Vector3& position) noexcept;
        [[nodiscard]] static bool IsRotationSuppressedForAnalysis(
            const Matrix3& rotation) noexcept;
        [[nodiscard]] static DerivedSizeState DecodeSizeForAnalysis(
            const Vector3& fullSize) noexcept;
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
            spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,spBaseObject& object) const override;
    };
}
