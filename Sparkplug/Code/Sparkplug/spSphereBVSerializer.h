#pragma once

// The native class and field diagnostics survive in both executables, but an
// exact original translation-unit path has not yet been recovered.

#include "spSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spSphereBV;
    class spSphereBVSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x7294634F;
        static constexpr spClassID TargetClassID = 0x390946D2;
        static constexpr float PositionSuppressionEpsilon = 0.001F;

        enum class Field : std::uint32_t
        {
            Position = 0,
            Radius = 1,
        };

        struct Vector3 final
        {
            float x = 0.0F;
            float y = 0.0F;
            float z = 0.0F;
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
            float radius = 0.0F;
        };

        spSphereBVSerializer() noexcept = default;
        ~spSphereBVSerializer() override;

        spSphereBVSerializer(const spSphereBVSerializer&) = delete;
        spSphereBVSerializer& operator=(const spSphereBVSerializer&) = delete;

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
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
        // PC43A2E0 scalar branches; raw values are copied without finite/range
        // filtering. The whole reader and field inspectors share this method.
        [[nodiscard]] static bool ReadScalarFieldForAnalysis(std::uint32_t fieldID,
            spStream& stream,spSphereBV& object);
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
            spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream& stream,
            const spBaseObject& object,std::string* error) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,
            spBaseObject& object) const override;
    };
}
