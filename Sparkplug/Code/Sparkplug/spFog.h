#pragma once

// Inferred declaration path. PC and PS2 factories, serializers and renderer
// consumers confirm the complete five-field runtime payload.

#include "../SparkBase/spBaseObject.h"

#include <cstdint>

namespace sparkplug::reconstruction
{
    class spFog final : public spBaseObject
    {
    public:
        enum class Type : std::uint32_t
        {
            Disabled = 0,
            Exponential = 1,
            ExponentialSquared = 2,
            Linear = 3,
        };

        static constexpr spClassID ClassID = 0x7AC95AEC;
        static constexpr std::uint32_t DefaultColorARGB = 0xFF000000;

        spFog() noexcept;
        ~spFog() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] Type GetTypeForAnalysis() const noexcept;
        void SetTypeForAnalysis(Type value) noexcept;
        [[nodiscard]] std::uint32_t GetColorARGBForAnalysis() const noexcept;
        void SetColorARGBForAnalysis(std::uint32_t value) noexcept;
        [[nodiscard]] float GetStartForAnalysis() const noexcept;
        void SetStartForAnalysis(float value) noexcept;
        [[nodiscard]] float GetEndForAnalysis() const noexcept;
        void SetEndForAnalysis(float value) noexcept;
        [[nodiscard]] float GetDensityForAnalysis() const noexcept;
        void SetDensityForAnalysis(float value) noexcept;

    private:
        Type type_ = Type::Disabled;
        std::uint32_t colorARGB_ = DefaultColorARGB;
        float start_ = 0.0F;
        float end_ = 1.0F;
        float density_ = 1.0F;
    };
}
