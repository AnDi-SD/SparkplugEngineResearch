#pragma once

// The exact basename spPS2RenderTarget.cpp survives in the PS2 executable;
// this header placement is inferred from the recovered module structure.

#include "../Sparkplug/spCubeRenderTarget.h"

namespace sparkplug::reconstruction
{
    class spPS2RenderTarget final : public spRenderTarget
    {
    public:
        static constexpr spClassID ClassID = 0x30A7021D;

        spPS2RenderTarget() noexcept = default;
        ~spPS2RenderTarget() override;

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
        [[nodiscard]] static std::uint32_t ToPS2FormatForAnalysis(
            eTBPixelFormat pixelFormat) noexcept;

    private:
        std::uint32_t textureManagerHandle_ = 0;
        bool active_ = false;
    };

    class spPS2CubeRenderTarget final : public spCubeRenderTarget
    {
    public:
        static constexpr spClassID ClassID = 0x5AE83884;

        spPS2CubeRenderTarget() noexcept = default;
        ~spPS2CubeRenderTarget() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Native PS2 renderer explicitly reports cube targets unsupported;
        // its concrete target hooks are successful no-ops.
        [[nodiscard]] bool Init(std::uint32_t width,
            std::uint32_t height,
            eTBPixelFormat pixelFormat) override;
        [[nodiscard]] bool ReinitTargetsForDeviceReset() override;
        void ReleaseTargetsForDeviceReset() noexcept override;
        [[nodiscard]] bool HasBackendTargetForAnalysis() const noexcept override;
    };
}
