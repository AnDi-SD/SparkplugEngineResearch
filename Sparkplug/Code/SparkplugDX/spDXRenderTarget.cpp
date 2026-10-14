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
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXRenderTargetRecord);
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

    std::uint32_t spDXRenderTarget::CalculateBackingTextureSizeForAnalysis(
        const sparkplug::analysis::host::RenderTargetSurfaceDescription&
            description) noexcept
    {
        // All operations deliberately remain uint32_t, matching the original
        // x86 wrapping arithmetic. Height is never consumed by PC 004CDB30.
        switch (description.format)
        {
        case 0x15:
        case 0x16:
            return description.width * 4u;
        case 0x17:
        case 0x1A:
            return description.width * 2u;
        case 0x31545844:
        {
            const auto blocks = description.width >> 2;
            return (blocks < 1u ? 1u : blocks) * 8u;
        }
        case 0x33545844:
        case 0x35545844:
        {
            const auto blocks = description.width >> 2;
            return (blocks < 1u ? 1u : blocks) * 16u;
        }
        default:
            // Original pass-through includes 0x18/0x19 and unknown formats.
            return description.width;
        }
    }

    bool spDXRenderTarget::ReadBackingTextureSizeForAnalysis(
        void* const context,
        const sparkplug::analysis::host::RenderTargetSurfaceDescriptionReader
            reader,
        std::uint32_t& size)
    {
        if (reader == nullptr)
        {
            return false;
        }
        sparkplug::analysis::host::RenderTargetSurfaceDescription description{};
        if (!reader(context, description))
        {
            return false;
        }
        size = CalculateBackingTextureSizeForAnalysis(description);
        return true;
    }
}
