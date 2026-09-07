#pragma once

// Inferred declaration path. The original serializer translation unit is
// proven as Code/Sparkplug/spLightDataSerializer.cpp, while no original
// spLight declaration/implementation path survives in the shipped binaries.

#include "spNode.h"

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spLightManager;
    class spLight : public spNode
    {
    public:
        enum class Type : std::uint32_t
        {
            Directional = 0,
            Point = 1,
            Spot = 2,
            Ambient = 3,
        };

        using ColorRGBA = std::array<float, 4>;

        static constexpr spClassID ClassID = 0x72444900;
        static constexpr float DefaultIntensity = 1.0F;
        static constexpr float DefaultRange = 200.0F;

        ~spLight() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        // The abstract native registration has no factory and its clone slot
        // returns null. Concrete spLightData supplies the allocating clone.
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] Type GetTypeForAnalysis() const noexcept;
        void SetTypeForAnalysis(Type value) noexcept;
        [[nodiscard]] const ColorRGBA& GetColorForAnalysis() const noexcept;
        void SetColorForAnalysis(const ColorRGBA& value) noexcept;
        [[nodiscard]] bool ProjectsShadowVolumeForAnalysis() const noexcept;
        void SetProjectsShadowVolumeForAnalysis(bool value) noexcept;
        [[nodiscard]] bool UsesAttenuationForAnalysis() const noexcept;
        void SetUsesAttenuationForAnalysis(bool value) noexcept;
        [[nodiscard]] float GetIntensityForAnalysis() const noexcept;
        void SetIntensityForAnalysis(float value) noexcept;
        [[nodiscard]] float GetRangeForAnalysis() const noexcept;
        void SetRangeForAnalysis(float value) noexcept;
        [[nodiscard]] float GetHotspotAngleForAnalysis() const noexcept;
        void SetHotspotAngleForAnalysis(float value) noexcept;
        [[nodiscard]] float GetFalloffAngleForAnalysis() const noexcept;
        void SetFalloffAngleForAnalysis(float value) noexcept;
        [[nodiscard]] bool IsLightEnabledForAnalysis() const noexcept;
        void SetLightEnabledForAnalysis(bool value) noexcept;
        // PC440640/471670 mark bit8 after every known light field, even equal values.
        void MarkLightDataDirtyForAnalysis() noexcept { flags_ |= 8u; }
        // Explicit borrowed scene+34 dependency. Binding alone does not imply
        // native Node attachment, registration or ownership (PC428C30/CP92).
        void SetSceneLightManagerForAnalysis(spLightManager* manager) noexcept
        { sceneLightManager_ = manager; }
        // PC428C30 clears dirty8 after Node world, then refreshes scene targets.
        [[nodiscard]] bool UpdateWorldForAnalysis(std::uint32_t inheritedFlags=0,
            const Matrix3* cameraOrientation=nullptr) noexcept override;

        // Native +0xDC PC / +0xE8 PS2 is copied but neither initialized by
        // spLight nor serialized by spLightDataSerializer. Keep it raw until
        // a consumer establishes its type and role.
        [[nodiscard]] std::uint32_t GetOpaqueRuntimeFieldBitsForAnalysis() const noexcept;
        void SetOpaqueRuntimeFieldBitsForAnalysis(std::uint32_t value) noexcept;

    protected:
        spLight() noexcept;

    private:
        spLightManager* sceneLightManager_ = nullptr;
        Type type_ = Type::Directional;
        ColorRGBA color_{1.0F, 1.0F, 1.0F, 1.0F};
        bool attenuationEnabled_ = false;
        float intensity_ = DefaultIntensity;
        std::uint32_t opaqueRuntimeFieldBits_ = 0;
        float range_ = DefaultRange;
        float hotspotAngle_ = 0.0F;
        float falloffAngle_ = 0.0F;
        bool projectShadowVolume_ = false;
        bool lightEnabled_ = true;
    };
}
