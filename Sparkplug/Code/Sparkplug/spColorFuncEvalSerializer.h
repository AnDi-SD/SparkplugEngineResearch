#pragma once

// The native class name and all field diagnostics survive in both executables,
// but an exact original translation-unit path has not yet been recovered.

#include "spSerializer.h"

#include <array>
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spColorFuncEvalSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x2CC46B90;
        static constexpr spClassID TargetClassID = 0x0BC70FE7;

        enum class Field : std::uint32_t
        {
            Color1 = 0,
            Color2 = 1,
            FunctionType = 2,
            Frequency = 3,
            Amplitude = 4,
            XOffset = 5,
            YOffset = 6,
            Pitch = 7,
        };

        enum class WireKind : std::uint32_t
        {
            ColorARGB,
            UInt32,
            Float32,
        };

        enum class DefaultRule : std::uint32_t
        {
            PlatformColor,
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
            std::uint32_t color1ARGB = 0;
            std::uint32_t color2ARGB = 0;
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

        using FieldSchema = std::array<FieldBinding, 8>;

        spColorFuncEvalSerializer() noexcept;
        ~spColorFuncEvalSerializer() override;

        spColorFuncEvalSerializer(const spColorFuncEvalSerializer&) = delete;
        spColorFuncEvalSerializer& operator=(
            const spColorFuncEvalSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept;
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
        [[nodiscard]] bool ReadColorFieldsForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,bool exact,std::string*) const;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;

        [[nodiscard]] static FieldSchema BuildFieldSchemaForAnalysis() noexcept;
        [[nodiscard]] static std::vector<Field> BuildWritePlanForAnalysis(
            const WriteShape& shape,
            std::uint32_t platformDefaultColorARGB);
        [[nodiscard]] static FrequencyState DecodeFrequencyForAnalysis(
            float wireFrequency) noexcept;
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
    };
}
