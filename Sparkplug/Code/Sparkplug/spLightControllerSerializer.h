#pragma once

// The class and all nine field diagnostics survive in both executables, but
// an exact original translation-unit path has not yet been recovered.

#include "spSerializer.h"

#include <array>
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spLightControllerSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x70573E5E;
        static constexpr spClassID TargetClassID = 0x10262533;
        static constexpr spClassID LightClassID = 0x72444900;

        enum class Field : std::uint32_t
        {
            Enabled = 0,
            Color1 = 1,
            Color2 = 2,
            Type = 3,
            Frequency = 4,
            Amplitude = 5,
            Offset = 6,
            Pitch = 7,
            Light = 8,
        };

        enum class WireKind : std::uint32_t
        {
            Boolean,
            ColorARGB,
            UInt32,
            Float32,
            Relationship,
        };

        enum class DefaultRule : std::uint32_t
        {
            Zero,
            PlatformColor,
            One,
            Always,
        };

        struct FieldBinding final
        {
            Field field;
            WireKind wireKind;
            DefaultRule defaultRule;
            std::uint32_t targetOffset;

            [[nodiscard]] bool operator==(const FieldBinding& other) const noexcept;
        };

        struct WriteShape final
        {
            bool enabled = false;
            std::uint32_t color1ARGB = 0;
            std::uint32_t color2ARGB = 0;
            std::uint32_t type = 0;
            float frequency = 1.0F;
            float amplitude = 1.0F;
            float offset = 0.0F;
            float pitch = 0.0F;
        };

        struct FrequencyState final
        {
            float frequency;
            float reciprocal;
        };

        using FieldSchema = std::array<FieldBinding, 9>;

        spLightControllerSerializer() noexcept = default;
        ~spLightControllerSerializer() override;

        spLightControllerSerializer(const spLightControllerSerializer&) = delete;
        spLightControllerSerializer& operator=(
            const spLightControllerSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept;

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
