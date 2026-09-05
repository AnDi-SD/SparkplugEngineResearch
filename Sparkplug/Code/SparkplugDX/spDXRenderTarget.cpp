#include "spDXRenderTarget.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord DXRenderTargetRecord{
            spDXRenderTarget::ClassID,
            spRenderTarget::ClassID,
            "spDXRenderTarget",
            &spRenderTarget::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool DXRenderTargetRegistered =
            spRTTIManager::Instance().Register(DXRenderTargetRecord);
    }

    spDXRenderTarget::~spDXRenderTarget() = default;

    const spRTTIRecord& spDXRenderTarget::StaticRTTI() noexcept
    {
        (void)DXRenderTargetRegistered;
        return DXRenderTargetRecord;
    }

    std::unique_ptr<spBaseObject> spDXRenderTarget::vfunc_10(
        spCloneManager&) const
    {
        return nullptr;
    }

    const spRTTIRecord& spDXRenderTarget::vfunc_18() const noexcept
    {
        return DXRenderTargetRecord;
    }

    bool spDXRenderTarget::Init(
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

        // Native code creates a D3D render-target texture, then level-zero
        // surface. The host reconstruction records the exact transition only.
        textureReady_ = true;
        surfaceReady_ = true;
        SetCommonStateForAnalysis(width, height, pixelFormat, true);
        return true;
    }

    bool spDXRenderTarget::ReinitTargetsForDeviceReset()
    {
        return spRenderTarget::ReinitTargetsForDeviceReset();
    }

    void spDXRenderTarget::ReleaseTargetsForDeviceReset() noexcept
    {
        surfaceReady_ = false;
        textureReady_ = false;
        spRenderTarget::ReleaseTargetsForDeviceReset();
    }

    bool spDXRenderTarget::HasBackendTargetForAnalysis() const noexcept
    {
        return textureReady_ && surfaceReady_;
    }

    bool spDXRenderTarget::IsSupportedFormatForAnalysis(
        const eTBPixelFormat pixelFormat) noexcept
    {
        switch (pixelFormat)
        {
        case eTBPixelFormat::Format0:
        case eTBPixelFormat::Format1:
        case eTBPixelFormat::Format3:
        case eTBPixelFormat::Format4:
        case eTBPixelFormat::Format5:
            return true;
        default:
            return false;
        }
    }

    std::uint32_t spDXRenderTarget::ToD3DFormatForAnalysis(
        const eTBPixelFormat pixelFormat) noexcept
    {
        switch (pixelFormat)
        {
        case eTBPixelFormat::Format0: return 0x15;
        case eTBPixelFormat::Format1: return 0x16;
        case eTBPixelFormat::Format3: return 0x17;
        case eTBPixelFormat::Format4: return 0x1A;
        case eTBPixelFormat::Format5: return 0x19;
        default: return 0;
        }
    }
}
