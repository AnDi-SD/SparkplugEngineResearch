#pragma once

// Exact original source path recovered from the PC executable:
// Z:\Sparkplug\Code\Sparkplug\spNodeSerializer.cpp

#include "spSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spNode;

    class spNodeSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x4545848A;
        static constexpr spClassID TargetClassID = 0x695C0F65;
        static constexpr float DefaultComparisonTolerance = 0.001F;

        enum class Field : std::uint32_t
        {
            Position = 0,
            Rotation = 1,
            Scale = 2,
            IsBone = 3,
            IsStatic = 4,
            Child = 5,
            BillboardAxis = 6,
            Collision = 7,
            IsAnimated = 8,
        };

        spNodeSerializer() noexcept = default;
        ~spNodeSerializer() override;

        spNodeSerializer(const spNodeSerializer&) = delete;
        spNodeSerializer& operator=(const spNodeSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Native secondary-interface slot: the serializer handles spNode.
        [[nodiscard]] virtual spClassID GetTargetClassIDForAnalysis() const noexcept;

        // Reconstructs the proven native write order and default suppression.
        // Collision fields are intentionally absent until spCollisionInfo is
        // represented by the portable spNode facade.
        [[nodiscard]] std::vector<Field> BuildKnownWritePlanForAnalysis(
            const spNode& node) const;
    };
}
