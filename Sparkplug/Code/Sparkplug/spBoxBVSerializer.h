#pragma once

// The native class and field diagnostics survive in both executables, but an
// exact original translation-unit path has not yet been recovered.

#include "spSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spBoxBV;
    class spBoxBVSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x48E43495;
        static constexpr spClassID TargetClassID = 0x7B4C0876;
        static constexpr float PositionSuppressionEpsilon = 0.001F;

        enum class Field : std::uint32_t
        {
            Position = 0,
            Size = 1,
        };

        struct Vector3 final
        {
            float x = 0.0F;
            float y = 0.0F;
            float z = 0.0F;

            [[nodiscard]] bool operator==(const Vector3& other) const noexcept;
        };

        struct FieldBinding final
        {
            Field field;
            std::uint32_t wireSourceOffset;
            std::uint32_t decodedMirrorOffset;
            bool alwaysWritten;

            [[nodiscard]] bool operator==(
                const FieldBinding& other) const noexcept;
        };

        struct WriteShape final
        {
            Vector3 position;
            Vector3 size;
        };

        struct DerivedSizeState final
        {
            Vector3 halfExtents;
            float boundingSphereRadius;
        };

        spBoxBVSerializer() noexcept = default;
        ~spBoxBVSerializer() override;

        spBoxBVSerializer(const spBoxBVSerializer&) = delete;
        spBoxBVSerializer& operator=(const spBoxBVSerializer&) = delete;

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
        [[nodiscard]] static DerivedSizeState DecodeSizeForAnalysis(
            const Vector3& fullSize) noexcept;
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
        // PC439470 scalar branches, including raw source bits and post-read
        // derived radius. No positivity or finite-input restriction is added.
        [[nodiscard]] static bool ReadScalarFieldForAnalysis(std::uint32_t fieldID,
            spStream& stream,spBoxBV& object);
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
            spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream& stream,
            const spBaseObject& object,std::string* error) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,
            spBaseObject& object) const override;
    };
}
