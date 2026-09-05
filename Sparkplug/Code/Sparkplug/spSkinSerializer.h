#pragma once

// Exact original PC translation-unit path recovered from diagnostics:
//   Z:\Sparkplug\Code\Sparkplug\spSkinSerializer.cpp
// The header path and ForAnalysis names are reconstruction choices.

#include "spModelSerializer.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spSkin;

    class spSkinSerializer final : public spModelSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x120D33C7;
        static constexpr spClassID TargetClassID = 0x681F2043;
        static constexpr spClassID BoneRelationshipClassID = 0x695C0F65;

        enum class Field : std::uint32_t
        {
            Skin = 0,
        };

        enum class PayloadSegment
        {
            WeightCount,
            BoneCount,
            BoneRelationship,
            InverseBindMatrix,
        };

        struct PayloadPlanEntry final
        {
            PayloadSegment segment;
            std::size_t elementCount;
            std::size_t byteCount;

            [[nodiscard]] bool operator==(
                const PayloadPlanEntry& other) const noexcept;
        };

        struct KnownWritePlan final
        {
            spModelSerializer::KnownWritePlan model;
            std::vector<Field> skinFields;
            std::vector<PayloadPlanEntry> payload;

            [[nodiscard]] bool operator==(const KnownWritePlan& other) const noexcept;
        };

        spSkinSerializer() noexcept = default;
        ~spSkinSerializer() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;

        [[nodiscard]] KnownWritePlan BuildKnownWritePlanForAnalysis(
            const spSkin& skin) const;
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
    };
}
