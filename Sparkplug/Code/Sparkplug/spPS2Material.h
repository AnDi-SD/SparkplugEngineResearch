#pragma once

// Inferred declaration path. The class is present only in the PS2 executable;
// its identity, direct base, complete layout and color accessors are native
// evidence. Host-side defaults intentionally avoid uninitialized storage.

#include "spMaterial.h"

#include <array>

namespace sparkplug::reconstruction
{
    class spPS2Material final : public spMaterial
    {
    public:
        using ColorRGBA = std::array<float, 4>;
        static constexpr spClassID ClassID = 0x0F507BC8;

        spPS2Material() noexcept;
        ~spPS2Material() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] const ColorRGBA& GetDiffuseColorForAnalysis() const noexcept;
        void SetDiffuseColorForAnalysis(const ColorRGBA& value) noexcept;
        [[nodiscard]] const ColorRGBA& GetAmbientColorForAnalysis() const noexcept;
        void SetAmbientColorForAnalysis(const ColorRGBA& value) noexcept;
        [[nodiscard]] const ColorRGBA& GetSpecularColorForAnalysis() const noexcept;
        void SetSpecularColorForAnalysis(const ColorRGBA& value) noexcept;
        [[nodiscard]] const ColorRGBA& GetEmissiveColorForAnalysis() const noexcept;
        void SetEmissiveColorForAnalysis(const ColorRGBA& value) noexcept;
        [[nodiscard]] float GetSpecularPowerForAnalysis() const noexcept;
        void SetSpecularPowerForAnalysis(float value) noexcept;

    private:
        ColorRGBA diffuse_{1.0F, 1.0F, 1.0F, 1.0F};
        ColorRGBA ambient_{0.0F, 0.0F, 0.0F, 1.0F};
        ColorRGBA specular_{1.0F, 1.0F, 1.0F, 1.0F};
        ColorRGBA emissive_{0.0F, 0.0F, 0.0F, 1.0F};
        float specularPower_ = 0.0F;
    };
}
