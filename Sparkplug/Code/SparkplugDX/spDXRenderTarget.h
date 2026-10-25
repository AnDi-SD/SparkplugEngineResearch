#pragma once

// Inferred header path. Exact implementation path recovered from PC:
// Z:\Sparkplug\Code\SparkplugDX\spDXRenderTarget.cpp.

#include "../Sparkplug/spRenderTarget.h"
#include "../../Analysis/Host/spRenderTargetSurfaceHost.h"
#include "spDXTextureResource.h"

namespace sparkplug::reconstruction
{
    class spDXRenderTarget : public spRenderTarget
    {
    public:
        static constexpr spClassID ClassID = 0x189A4642;

        ~spDXRenderTarget() override;

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

        // Original PC 004CDB30: GetDesc on the acquired level-zero surface,
        // then width-only format arithmetic. Original method name unknown.
        // The result is the value passed to the backing texture initializer,
        // not a width*height allocation or a mip-chain storage estimate.
        [[nodiscard]] static std::uint32_t CalculateBackingTextureSizeForAnalysis(
            const sparkplug::analysis::host::RenderTargetSurfaceDescription&
                description) noexcept;
        [[nodiscard]] static bool ReadBackingTextureSizeForAnalysis(
            void* context,
            sparkplug::analysis::host::RenderTargetSurfaceDescriptionReader reader,
            std::uint32_t& size);

        // Actual004CDC70/004CDD60/004CDBE0 on declared existing target/backing
        // state. The bool-only portable facade above remains a CPU model.
        [[nodiscard]] static DXResourceResultForAnalysis InitializeDeviceTargetForAnalysis(
            DXRenderTargetStateForAnalysis&, DXResourceHandleForAnalysis device,
            std::uint32_t width, std::uint32_t height, std::uint32_t pixelFormat,
            const DXTextureResourceAPIForAnalysis&);
        [[nodiscard]] static bool ReleaseDeviceTargetForAnalysis(
            DXRenderTargetStateForAnalysis&, const DXTextureResourceAPIForAnalysis&);
        [[nodiscard]] static DXResourceResultForAnalysis ReinitializeDeviceTargetForAnalysis(
            DXRenderTargetStateForAnalysis&, DXResourceHandleForAnalysis device,
            const DXTextureResourceAPIForAnalysis&);

    protected:
        using spRenderTarget::spRenderTarget;

    private:
        bool textureReady_ = false;
        bool surfaceReady_ = false;
    };
}
