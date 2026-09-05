#pragma once

#include "spMaterialRenderTargetTexture.h"

namespace sparkplug::reconstruction
{
    class spMaterialCubeMapTexture final
        : public spMaterialRenderTargetTexture
    {
    public:
        static constexpr spClassID ClassID = 0x1C3B499A;
        static constexpr std::uint32_t CubeFaceCount = 6;

        spMaterialCubeMapTexture() = default;
        ~spMaterialCubeMapTexture() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] RenderKind GetRenderKindForAnalysis()
            const noexcept override;
        [[nodiscard]] std::unique_ptr<spMaterialRenderTargetTexture>
            CopyRenderTargetTextureForAnalysis() const override;

        [[nodiscard]] spBaseObject* GetSourceRenderNodeForAnalysis()
            const noexcept;
        void SetSourceRenderNodeForAnalysis(spBaseObject* source) noexcept;
        [[nodiscard]] spBaseObject* GetOwnedCameraForAnalysis() const noexcept;
        void SetOwnedCameraForAnalysis(spBaseObject* camera) noexcept;
        [[nodiscard]] std::uint32_t GetNumFacesToRenderPerTickForAnalysis()
            const noexcept;
        void SetNumFacesToRenderPerTickForAnalysis(std::uint32_t count) noexcept;
        [[nodiscard]] std::uint32_t GetCurrentFaceForAnalysis() const noexcept;
        void AdvanceFacesForAnalysis() noexcept;

    private:
        spBaseObject* sourceRenderNode_ = nullptr;
        spBaseObject* ownedCamera_ = nullptr;
        std::uint32_t facesPerTick_ = CubeFaceCount;
        std::uint32_t currentFace_ = 0;
    };
}
