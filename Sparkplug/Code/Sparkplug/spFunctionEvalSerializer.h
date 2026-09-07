#pragma once

// The native class name and field diagnostics survive in both executables,
// but an exact original translation-unit path has not yet been recovered.

#include "spSerializer.h"

#include <array>
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spFunctionEval;
    class spFunctionEvalSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x1D2A151D;
        static constexpr spClassID TargetClassID = 0x9450E590;

        enum class Field : std::uint32_t
        {
            FunctionType = 0,
            Frequency = 1,
            Amplitude = 2,
            XOffset = 3,
            YOffset = 4,
            Pitch = 5,
        };

        enum class WireKind : std::uint32_t
        {
            UInt32,
            Float32,
        };

        enum class DefaultRule : std::uint32_t
        {
            Zero,
            One,
        };

        struct FieldBinding final
        {
            Field field;
            WireKind wireKind;
            DefaultRule defaultRule;
            std::uint32_t targetOffset;

            [[nodiscard]] bool operator==(
                const FieldBinding& other) const noexcept;
        };

        struct WriteShape final
        {
            std::uint32_t functionType = 0;
            float frequency = 1.0F;
            float amplitude = 1.0F;
            float xOffset = 0.0F;
            float yOffset = 0.0F;
            float pitch = 0.0F;
        };

        struct FrequencyState final
        {
            float frequency;
            float reciprocal;
        };

        using FieldSchema = std::array<FieldBinding, 6>;

        spFunctionEvalSerializer() noexcept;
        ~spFunctionEvalSerializer() override;

        spFunctionEvalSerializer(const spFunctionEvalSerializer&) = delete;
        spFunctionEvalSerializer& operator=(
            const spFunctionEvalSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept;
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,spBaseObject&,std::string*) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;
        // Packed TransFunction/Color sections share one outer envelope.
        [[nodiscard]] bool ReadFunctionFieldsForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,spBaseObject&,bool requireExactEnd,std::string*) const;
        // ColorFunc remaps IDs2..7 to this same six-field scalar state decoder.
        [[nodiscard]] static bool ApplyRawStateFieldForAnalysis(spFunctionEval&,std::uint32_t id,std::uint32_t raw) noexcept;

        [[nodiscard]] static FieldSchema BuildFieldSchemaForAnalysis() noexcept;
        [[nodiscard]] static std::vector<Field> BuildWritePlanForAnalysis(
            const WriteShape& shape);
        [[nodiscard]] static FrequencyState DecodeFrequencyForAnalysis(
            float wireFrequency) noexcept;
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
    };
}
