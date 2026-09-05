#pragma once

// The PS2 executable preserves the original file name
// "spModelSerializer.cpp". No containing source-tree path or header survives.

#include "spRenderableSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spModel;

    class spModelSerializer : public spRenderableSerializer
    {
    public:
        static constexpr spClassID ClassID = 0xDB55C34A;
        static constexpr spClassID TargetClassID = 0x763277DB;
        static constexpr spClassID MeshRelationshipClassID = 0x3F077B6C;

        enum class Field : std::uint32_t
        {
            Base = 0,
            ProjectionGroup = 1,
        };

        struct KnownWritePlan final
        {
            std::vector<spRenderableSerializer::Field> renderableFields;
            std::vector<Field> modelFields;

            [[nodiscard]] bool operator==(const KnownWritePlan& other) const noexcept;
        };

        spModelSerializer() noexcept = default;
        ~spModelSerializer() override;

        spModelSerializer(const spModelSerializer&) = delete;
        spModelSerializer& operator=(const spModelSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;

        [[nodiscard]] KnownWritePlan BuildKnownWritePlanForAnalysis(
            const spModel& model) const;
    };
}
