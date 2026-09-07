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
    class spAnimTexController;
    class spUVController;

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
        // Native41E870 retains one edge, same-pointer assignment is a no-op.
        // Shared owner is the canonical host equivalent; raw overload remains
        // explicitly borrowed for legacy analysis fixtures using stack objects.
        void SetOwnedFallBackTextureForAnalysis(std::shared_ptr<spTexture> texture) noexcept;
        [[nodiscard]] const std::shared_ptr<spTexture>& GetFallBackTextureOwnerForAnalysis() const noexcept{return fallbackOwner_;}
        void SetAnimTextureControllerForAnalysis(spBaseObject* controller) noexcept;
        void SetOwnedAnimTextureControllerForAnalysis(std::shared_ptr<spAnimTexController> controller) noexcept;
        [[nodiscard]] const auto& GetAnimTextureControllerOwnerForAnalysis() const noexcept{return animationOwner_;}
        [[nodiscard]] bool UpdateTextureAnimationForAnalysis();
        void SetUVControllerForAnalysis(spBaseObject* controller) noexcept;
        void SetOwnedUVControllerForAnalysis(std::shared_ptr<spUVController> controller) noexcept;
        [[nodiscard]] const auto& GetUVControllerOwnerForAnalysis() const noexcept{return uvOwner_;}
        // Original467B70 only recomputes UV when its two clocks differ;
        // submitting the matrix to the backend is a separate renderer boundary.
        [[nodiscard]] bool UpdateUVAnimationForAnalysis();
        using UVSubmitForAnalysis=bool (*)(void*,std::uint32_t,const std::array<float,UVTransformValueCount>&);
        // PC467B70: conditional UV update, submit this holder's matrix, then
        // unconditional AnimTex update if present. Backend result is ignored.
        [[nodiscard]] bool UpdateForRenderForAnalysis(std::uint32_t stage,UVSubmitForAnalysis submit,void* context);
        void SetTextureStateForAnalysis(std::size_t index,
            std::uint32_t value) noexcept;
        void SetStaticUVTransformForAnalysis(
            const std::array<float, UVTransformValueCount>& transform) noexcept;
        void ClearStaticUVTransformForAnalysis() noexcept;

    private:
        std::array<std::uint32_t, PS2TextureStateCount> textureStates_{};
        spTexture* fallbackTexture_ = nullptr;
        std::shared_ptr<spTexture> fallbackOwner_;
        spBaseObject* animationController_ = nullptr;
        std::shared_ptr<spAnimTexController> animationOwner_;
        std::array<float, UVTransformValueCount> uvTransform_{};
        bool hasStaticUV_ = false;
        spBaseObject* uvController_ = nullptr;
        std::shared_ptr<spUVController> uvOwner_;
    };
}
