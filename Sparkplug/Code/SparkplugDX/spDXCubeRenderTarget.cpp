#include "spDXCubeRenderTarget.h"

#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXCubeRenderTarget()
        {
            return std::make_unique<spDXCubeRenderTarget>();
        }

        const spRTTIRecord DXCubeRenderTargetRecord{
            spDXCubeRenderTarget::ClassID,
            spCubeRenderTarget::ClassID,
            "spDXCubeRenderTarget",
            &spCubeRenderTarget::StaticRTTI(),
            &CreateDXCubeRenderTarget,
            nullptr,
        };

        const bool DXCubeRenderTargetRegistered =
            spRTTIManager::Instance().Register(DXCubeRenderTargetRecord);
    }

    spDXCubeRenderTarget::~spDXCubeRenderTarget() = default;

    const spRTTIRecord& spDXCubeRenderTarget::StaticRTTI() noexcept
    {
        (void)DXCubeRenderTargetRegistered;
        return DXCubeRenderTargetRecord;
    }

    std::unique_ptr<spBaseObject> spDXCubeRenderTarget::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXCubeRenderTarget>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    const spRTTIRecord& spDXCubeRenderTarget::vfunc_18() const noexcept
    {
        return DXCubeRenderTargetRecord;
    }

    bool spDXCubeRenderTarget::Init(
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
        cubeTextureReady_ = true;
        facesReady_.fill(true);
        SetCommonStateForAnalysis(width, height, pixelFormat, true);
        return true;
    }

    bool spDXCubeRenderTarget::ReinitTargetsForDeviceReset()
    {
        return spRenderTarget::ReinitTargetsForDeviceReset();
    }

    void spDXCubeRenderTarget::ReleaseTargetsForDeviceReset() noexcept
    {
        facesReady_.fill(false);
        cubeTextureReady_ = false;
        spRenderTarget::ReleaseTargetsForDeviceReset();
    }

    bool spDXCubeRenderTarget::HasBackendTargetForAnalysis() const noexcept
    {
        return cubeTextureReady_
            && std::all_of(facesReady_.begin(), facesReady_.end(),
                [](const bool ready) { return ready; });
    }

    bool spDXCubeRenderTarget::IsSupportedFormatForAnalysis(
        const eTBPixelFormat pixelFormat) noexcept
    {
        switch (pixelFormat)
        {
        case eTBPixelFormat::Format0:
        case eTBPixelFormat::Format1:
        case eTBPixelFormat::Format3:
        case eTBPixelFormat::Format4:
            return true;
        default:
            return false;
        }
    }

    std::uint32_t spDXCubeRenderTarget::ToD3DFormatForAnalysis(
        const eTBPixelFormat pixelFormat) noexcept
    {
        switch (pixelFormat)
        {
        case eTBPixelFormat::Format0: return 0x15;
        case eTBPixelFormat::Format1: return 0x16;
        case eTBPixelFormat::Format3: return 0x17;
        case eTBPixelFormat::Format4: return 0x1A;
        default: return 0;
        }
    }

    std::size_t spDXCubeRenderTarget::GetReadyFaceCountForAnalysis()
        const noexcept
    {
        return static_cast<std::size_t>(std::count(
            facesReady_.begin(), facesReady_.end(), true));
    }
}
