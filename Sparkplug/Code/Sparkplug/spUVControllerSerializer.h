#pragma once

// The class and serializer diagnostics survive in both executables, but an
// exact original translation-unit path has not yet been recovered.

#include "spSerializer.h"

#include <array>
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spUVControllerSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x591224D0;
        static constexpr spClassID TargetClassID = 0x1C0053D6;
        static constexpr std::uint32_t EmbeddedTransformOffset = 0x4C;

        enum class Field : std::uint32_t
        {
            UVController = 0,
        };

        enum class EvaluatorRole : std::uint32_t
        {
            TranslationX,
            TranslationY,
            TranslationZ,
            ScaleX,
            ScaleY,
            ScaleZ,
            Rotation,
        };

        enum class VectorRole : std::uint32_t
        {
            UVPivot,
            RotationAxis,
        };

        struct EvaluatorBinding final
        {
            EvaluatorRole role;
            std::uint32_t transformOffset;
            std::uint32_t targetOffset;

            [[nodiscard]] bool operator==(
                const EvaluatorBinding& other) const noexcept;
        };

        struct VectorBinding final
        {
            VectorRole role;
            std::uint32_t transformOffset;
            std::uint32_t targetOffset;

            [[nodiscard]] bool operator==(
                const VectorBinding& other) const noexcept;
        };

        using EvaluatorPlan = std::array<EvaluatorBinding, 7>;
        using VectorPlan = std::array<VectorBinding, 2>;

        spUVControllerSerializer() noexcept;
        ~spUVControllerSerializer() override;

        spUVControllerSerializer(const spUVControllerSerializer&) = delete;
        spUVControllerSerializer& operator=(
            const spUVControllerSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept;
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,spBaseObject&,std::string*) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;

        [[nodiscard]] static std::vector<Field> BuildWritePlanForAnalysis();
        [[nodiscard]] static EvaluatorPlan BuildEvaluatorPlanForAnalysis() noexcept;
        [[nodiscard]] static VectorPlan BuildVectorPlanForAnalysis() noexcept;
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
    };
}
