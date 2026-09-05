#pragma once

// The class name and serializer diagnostics survive in both executables, but
// an exact original translation-unit path has not yet been recovered.

#include "spSerializer.h"

#include <array>
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spMatColorControllerSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x0F881A36;
        static constexpr spClassID TargetClassID = 0x4C633E85;

        enum class Field : std::uint32_t
        {
            MaterialColorController = 0,
        };

        enum class EvaluatorRole : std::uint32_t
        {
            Ambient,
            Diffuse,
            Specular,
            Emissive,
            Alpha,
        };

        enum class EvaluatorKind : std::uint32_t
        {
            ColorFunctional,
            Functional,
        };

        struct EvaluatorBinding final
        {
            EvaluatorRole role;
            EvaluatorKind kind;
            std::uint32_t targetOffset;

            [[nodiscard]] bool operator==(
                const EvaluatorBinding& other) const noexcept;
        };

        using EvaluatorPlan = std::array<EvaluatorBinding, 5>;

        spMatColorControllerSerializer() noexcept = default;
        ~spMatColorControllerSerializer() override;

        spMatColorControllerSerializer(const spMatColorControllerSerializer&) = delete;
        spMatColorControllerSerializer& operator=(
            const spMatColorControllerSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept;

        [[nodiscard]] static std::vector<Field> BuildWritePlanForAnalysis();
        [[nodiscard]] static EvaluatorPlan BuildEvaluatorPlanForAnalysis() noexcept;
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
    };
}
