#pragma once

// Inferred declaration path. The native class name, RTTI relationship and
// serializer-facing getter spellings survive; no original header path does.

#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spTexture;

    class spMaterialTexture : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x694E6975;
        static constexpr std::size_t PCTextureStateCount = 9;
        static constexpr std::size_t PS2TextureStateCount = 12;
        static constexpr std::size_t UVTransformValueCount = 9;

        spMaterialTexture() noexcept;
        ~spMaterialTexture() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] virtual spTexture* GetTextureForAnalysis() const noexcept;
        [[nodiscard]] spTexture* GetFallBackTextureForAnalysis() const noexcept;
        [[nodiscard]] spBaseObject* GetAnimTextureControllerForAnalysis()
            const noexcept;
        [[nodiscard]] spBaseObject* GetUVControllerForAnalysis() const noexcept;
        [[nodiscard]] const std::array<std::uint32_t, PS2TextureStateCount>&
            GetTextureStatesForAnalysis() const noexcept;
        [[nodiscard]] const std::array<float, UVTransformValueCount>&
            GetUVTransformForAnalysis() const noexcept;
        [[nodiscard]] bool HasStaticTransformForAnalysis() const noexcept;

        void SetFallBackTextureForAnalysis(spTexture* texture) noexcept;
        void SetAnimTextureControllerForAnalysis(spBaseObject* controller) noexcept;
        void SetUVControllerForAnalysis(spBaseObject* controller) noexcept;
        void SetTextureStateForAnalysis(std::size_t index,
            std::uint32_t value) noexcept;
        void SetStaticUVTransformForAnalysis(
            const std::array<float, UVTransformValueCount>& transform) noexcept;
        void ClearStaticUVTransformForAnalysis() noexcept;

    private:
        std::array<std::uint32_t, PS2TextureStateCount> textureStates_{};
        spTexture* fallbackTexture_ = nullptr;
        spBaseObject* animationController_ = nullptr;
        std::array<float, UVTransformValueCount> uvTransform_{};
        bool hasStaticUV_ = false;
        spBaseObject* uvController_ = nullptr;
    };
}
