#pragma once

// The class and serializer diagnostics survive in both executables, but an
// exact original translation-unit path has not yet been recovered.

#include "spSerializer.h"

#include <array>
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spAnimTexControllerSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x77793754;
        static constexpr spClassID TargetClassID = 0x16FB0E47;
        static constexpr spClassID TextureRelationshipClassID = 0x2F281E13;

        enum class Field : std::uint32_t
        {
            ControllerBase = 0,
        };

        enum class PayloadSegment : std::uint32_t
        {
            FrameCount,
            TimeArray,
            TextureRelationships,
        };

        struct SegmentPlan final
        {
            PayloadSegment segment;
            std::uint32_t elementCount;
            std::uint64_t fixedByteCount;
            bool variableLength;

            [[nodiscard]] bool operator==(const SegmentPlan& other) const noexcept;
        };

        using PayloadPlan = std::array<SegmentPlan, 3>;

        spAnimTexControllerSerializer() noexcept = default;
        ~spAnimTexControllerSerializer() override;

        spAnimTexControllerSerializer(const spAnimTexControllerSerializer&) = delete;
        spAnimTexControllerSerializer& operator=(
            const spAnimTexControllerSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept;

        [[nodiscard]] static std::vector<Field> BuildWritePlanForAnalysis();
        [[nodiscard]] static PayloadPlan BuildPayloadPlanForAnalysis(
            std::uint32_t frameCount) noexcept;
        [[nodiscard]] static bool HasConsistentTrackShapeForAnalysis(
            std::uint32_t timeCount,
            std::uint32_t textureRelationshipCount) noexcept;
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
    };
}
