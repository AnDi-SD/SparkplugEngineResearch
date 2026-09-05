#pragma once

// Inferred header path. Exact implementation path recovered from PC:
// Z:\Sparkplug\Code\SparkplugDX\spDXRenderTarget.cpp.

#include "../Sparkplug/spRenderTarget.h"

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

    protected:
        using spRenderTarget::spRenderTarget;

    private:
        bool textureReady_ = false;
        bool surfaceReady_ = false;
    };
}
