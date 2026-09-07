#include "spPS2RenderTarget.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2RenderTarget()
        {
            return std::make_unique<spPS2RenderTarget>();
        }

        std::unique_ptr<spBaseObject> CreatePS2CubeRenderTarget()
        {
            return std::make_unique<spPS2CubeRenderTarget>();
        }

        const spRTTIRecord PS2RenderTargetRecord{
            spPS2RenderTarget::ClassID,
            spRenderTarget::ClassID,
            "spPS2RenderTarget",
            &spRenderTarget::StaticRTTI(),
            &CreatePS2RenderTarget,
            nullptr,
        };

        const spRTTIRecord PS2CubeRenderTargetRecord{
            spPS2CubeRenderTarget::ClassID,
            spCubeRenderTarget::ClassID,
            "spPS2CubeRenderTarget",
            &spCubeRenderTarget::StaticRTTI(),
            &CreatePS2CubeRenderTarget,
            nullptr,
        };

        const bool PS2RenderTargetRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(PS2RenderTargetRecord);
        const bool PS2CubeRenderTargetRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(PS2CubeRenderTargetRecord);
    }

    spPS2RenderTarget::~spPS2RenderTarget() = default;

    const spRTTIRecord& spPS2RenderTarget::StaticRTTI() noexcept
    {
        (void)PS2RenderTargetRegistered;
        return PS2RenderTargetRecord;
    }

    std::unique_ptr<spBaseObject> spPS2RenderTarget::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2RenderTarget>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    const spRTTIRecord& spPS2RenderTarget::vfunc_18() const noexcept
    {
        return PS2RenderTargetRecord;
    }

    bool spPS2RenderTarget::Init(
        const std::uint32_t width,
        const std::uint32_t height,
        const eTBPixelFormat pixelFormat)
    {
        ReleaseTargetsForDeviceReset();
        if (width == 0 || height == 0
            || !IsSupportedFormatForAnalysis(pixelFormat))
        {
            return false;
        }

        // Native code obtains this handle from the PS2 texture manager. A
        // non-zero token is sufficient for the portable lifecycle contract.
        textureManagerHandle_ = 1;
        active_ = true;
        SetCommonStateForAnalysis(width, height, pixelFormat, true);
        return true;
    }

    bool spPS2RenderTarget::ReinitTargetsForDeviceReset()
    {
        return spRenderTarget::ReinitTargetsForDeviceReset();
    }

    void spPS2RenderTarget::ReleaseTargetsForDeviceReset() noexcept
    {
        active_ = false;
        textureManagerHandle_ = 0;
        spRenderTarget::ReleaseTargetsForDeviceReset();
    }

    bool spPS2RenderTarget::HasBackendTargetForAnalysis() const noexcept
    {
        return active_ && textureManagerHandle_ != 0;
    }

    bool spPS2RenderTarget::IsSupportedFormatForAnalysis(
        const eTBPixelFormat pixelFormat) noexcept
    {
        return pixelFormat == eTBPixelFormat::Format0
            || pixelFormat == eTBPixelFormat::Format1
            || pixelFormat == eTBPixelFormat::Format5;
    }

    std::uint32_t spPS2RenderTarget::ToPS2FormatForAnalysis(
        const eTBPixelFormat pixelFormat) noexcept
    {
        if (pixelFormat == eTBPixelFormat::Format0)
        {
            return 0;
        }
        if (pixelFormat == eTBPixelFormat::Format1)
        {
            return 1;
        }
        return pixelFormat == eTBPixelFormat::Format5 ? 10U : 0U;
    }

    const spRTTIRecord& spPS2CubeRenderTarget::StaticRTTI() noexcept
    {
        (void)PS2CubeRenderTargetRegistered;
        return PS2CubeRenderTargetRecord;
    }

    std::unique_ptr<spBaseObject> spPS2CubeRenderTarget::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2CubeRenderTarget>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    const spRTTIRecord& spPS2CubeRenderTarget::vfunc_18() const noexcept
    {
        return PS2CubeRenderTargetRecord;
    }

    bool spPS2CubeRenderTarget::Init(
        const std::uint32_t width,
        const std::uint32_t height,
        const eTBPixelFormat pixelFormat)
    {
        SetCommonStateForAnalysis(width, height, pixelFormat, false);
        return true;
    }

    bool spPS2CubeRenderTarget::ReinitTargetsForDeviceReset()
    {
        return true;
    }

    void spPS2CubeRenderTarget::ReleaseTargetsForDeviceReset() noexcept
    {
        spRenderTarget::ReleaseTargetsForDeviceReset();
    }

    bool spPS2CubeRenderTarget::HasBackendTargetForAnalysis() const noexcept
    {
        return false;
    }
}
