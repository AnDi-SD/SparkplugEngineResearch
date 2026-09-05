#pragma once

// Inferred declaration path. Runtime class identity and member offsets are
// independently confirmed by PC/PS2 serializer and render paths.

#include "spMaterial.h"

#include <array>

namespace sparkplug::reconstruction
{
    class spMaterialData final : public spMaterial
    {
    public:
        using ColorRGBA = std::array<float, 4>;

        static constexpr spClassID ClassID = 0x6160348B;

        spMaterialData() noexcept;
        ~spMaterialData() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] const ColorRGBA& GetAmbientColorForAnalysis()
            const noexcept;
        void SetAmbientColorForAnalysis(const ColorRGBA& value) noexcept;
        [[nodiscard]] const ColorRGBA& GetDiffuseColorForAnalysis()
            const noexcept;
        void SetDiffuseColorForAnalysis(const ColorRGBA& value) noexcept;
        [[nodiscard]] const ColorRGBA& GetSpecularColorForAnalysis()
            const noexcept;
        void SetSpecularColorForAnalysis(const ColorRGBA& value) noexcept;
        [[nodiscard]] const ColorRGBA& GetEmissiveColorForAnalysis()
            const noexcept;
        void SetEmissiveColorForAnalysis(const ColorRGBA& value) noexcept;
        [[nodiscard]] float GetSpecularPowerForAnalysis() const noexcept;
        void SetSpecularPowerForAnalysis(float value) noexcept;

    private:
        // Native offsets relative to the complete object are +0x80, +0x90,
        // +0xA0, +0xB0 and +0xC0 respectively.
        ColorRGBA diffuse_{1.0F, 1.0F, 1.0F, 1.0F};
        ColorRGBA ambient_{0.0F, 0.0F, 0.0F, 1.0F};
        ColorRGBA specular_{1.0F, 1.0F, 1.0F, 1.0F};
        ColorRGBA emissive_{0.0F, 0.0F, 0.0F, 1.0F};
        float specularPower_ = 0.0F;
    };
}
