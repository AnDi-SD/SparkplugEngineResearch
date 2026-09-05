#pragma once

// Inferred declaration path. Native diagnostics prove this class mediates
// material layers and per-render-pass target slots. Its RTTI factory is null.

#include "spMaterialTexture.h"
#include "spRenderTarget.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spMaterialRenderTargetTexture : public spMaterialTexture
    {
    public:
        static constexpr spClassID ClassID = 0x535D1473;
        static constexpr std::uint32_t DefaultMaxRecursionLevel = 1;
        static constexpr std::uint32_t DefaultDimension = 256;

        ~spMaterialRenderTargetTexture() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spTexture* GetTextureForAnalysis() const noexcept override;

        [[nodiscard]] std::uint32_t GetMaxRecursionLevelForAnalysis()
            const noexcept;
        [[nodiscard]] std::uint32_t GetCurrentRecursionLevelForAnalysis()
            const noexcept;
        [[nodiscard]] std::uint32_t GetWidthForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetHeightForAnalysis() const noexcept;
        [[nodiscard]] eTBPixelFormat GetPixelFormatForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetRenderTargetSlotCountForAnalysis()
            const noexcept;

        void SetMaxRecursionLevelForAnalysis(std::uint32_t level) noexcept;
        void SetDimensionsForAnalysis(
            std::uint32_t width, std::uint32_t height) noexcept;
        void SetPixelFormatForAnalysis(eTBPixelFormat format) noexcept;
        [[nodiscard]] bool SetRenderTargetForAnalysis(
            std::size_t slot, spRenderTarget* target) noexcept;
        [[nodiscard]] bool TryEnterRenderForAnalysis() noexcept;
        void LeaveRenderForAnalysis() noexcept;

        void ReleaseRenderTargetsForDeviceResetForAnalysis() noexcept;
        [[nodiscard]] bool ReinitRenderTargetsForDeviceResetForAnalysis();

        // The two final native virtual slots are implemented by both concrete
        // leaves. Original source-level signatures remain unknown, so the
        // portable seam exposes only their established semantic distinction.
        enum class RenderKind : std::uint8_t
        {
            CameraView,
            CubeMap,
        };
        [[nodiscard]] virtual RenderKind GetRenderKindForAnalysis()
            const noexcept = 0;
        [[nodiscard]] virtual std::unique_ptr<spMaterialRenderTargetTexture>
            CopyRenderTargetTextureForAnalysis() const = 0;

    protected:
        spMaterialRenderTargetTexture();

    private:
        std::uint32_t maxRecursionLevel_ = DefaultMaxRecursionLevel;
        std::uint32_t currentRecursionLevel_ = 0;
        std::uint32_t width_ = DefaultDimension;
        std::uint32_t height_ = DefaultDimension;
        eTBPixelFormat pixelFormat_ = eTBPixelFormat::Format0;
        std::vector<spRenderTarget*> renderTargets_;
    };
}
