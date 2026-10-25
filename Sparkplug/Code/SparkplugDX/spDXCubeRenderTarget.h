#pragma once

// Inferred header path. Exact implementation path recovered from PC:
// Z:\Sparkplug\Code\SparkplugDX\spDXCubeRenderTarget.cpp.

#include "../Sparkplug/spCubeRenderTarget.h"
#include "../../Analysis/Host/spRenderTargetSurfaceHost.h"
#include "spDXTextureResource.h"

#include <array>

namespace sparkplug::reconstruction
{
    class spDXCubeRenderTarget final : public spCubeRenderTarget
    {
    public:
        static constexpr spClassID ClassID = 0x5249684C;

        spDXCubeRenderTarget() noexcept = default;
        ~spDXCubeRenderTarget() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] bool Init(std::uint32_t width,
            std::uint32_t height,
            eTBPixelFormat pixelFormat) override;
        [[nodiscard]] bool ReinitTargetsForDeviceReset() override;
        void ReleaseTargetsForDeviceReset() noexcept override;
        [[nodiscard]] bool HasBackendTargetForAnalysis() const noexcept override;

        [[nodiscard]] static bool IsSupportedFormatForAnalysis(
            eTBPixelFormat pixelFormat) noexcept;
        [[nodiscard]] static std::uint32_t ToD3DFormatForAnalysis(
            eTBPixelFormat pixelFormat) noexcept;
        [[nodiscard]] std::size_t GetReadyFaceCountForAnalysis() const noexcept;

        // Original PC 004AC300 describes face zero once. Its width-only
        // format arithmetic is the ordinary-target result multiplied by six.
        // Original method name remains unknown; all products wrap at 32 bits.
        [[nodiscard]] static std::uint32_t CalculateBackingTextureSizeForAnalysis(
            const sparkplug::analysis::host::RenderTargetSurfaceDescription&
                description) noexcept;
        [[nodiscard]] static bool ReadBackingTextureSizeForAnalysis(
            void* context,
            sparkplug::analysis::host::RenderTargetSurfaceDescriptionReader reader,
            std::uint32_t& size);

        [[nodiscard]] static DXResourceResultForAnalysis InitializeDeviceTargetForAnalysis(
            DXCubeRenderTargetStateForAnalysis&, DXResourceHandleForAnalysis device,
            std::uint32_t width, std::uint32_t height, std::uint32_t pixelFormat,
            DXTextureResourceGlobalsForAnalysis&, const DXTextureResourceAPIForAnalysis&);
        [[nodiscard]] static bool ReleaseDeviceTargetForAnalysis(
            DXCubeRenderTargetStateForAnalysis&, const DXTextureResourceAPIForAnalysis&);
        [[nodiscard]] static DXResourceResultForAnalysis ReinitializeDeviceTargetForAnalysis(
            DXCubeRenderTargetStateForAnalysis&, DXResourceHandleForAnalysis device,
            DXTextureResourceGlobalsForAnalysis&, const DXTextureResourceAPIForAnalysis&);

    private:
        bool cubeTextureReady_ = false;
        std::array<bool, FaceCount> facesReady_{};
    };
}
