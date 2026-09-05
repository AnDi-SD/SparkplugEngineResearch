#include "spRenderTarget.h"
#include "spRenderTargetManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord RenderTargetRecord{
            spRenderTarget::ClassID,
            spResource::ClassID,
            "spRenderTarget",
            &spResource::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool RenderTargetRegistered =
            spRTTIManager::Instance().Register(RenderTargetRecord);
    }

    spRenderTarget::spRenderTarget(
        const std::uint32_t width,
        const std::uint32_t height,
        const eTBPixelFormat pixelFormat) noexcept
        : width_(width), height_(height), pixelFormat_(pixelFormat)
    {
    }

    spRenderTarget::~spRenderTarget()
    {
        if (auto* const manager = spRenderTargetManager::GetInstance();
            manager != nullptr)
        {
            (void)manager->DeactivateTargetForAnalysis(*this);
        }
    }

    const spRTTIRecord& spRenderTarget::StaticRTTI() noexcept
    {
        (void)RenderTargetRegistered;
        return RenderTargetRecord;
    }

    std::unique_ptr<spBaseObject> spRenderTarget::vfunc_10(
        spCloneManager&) const
    {
        return nullptr;
    }

    bool spRenderTarget::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* const target = dynamic_cast<spRenderTarget*>(&destination);
        if (target == nullptr
            || !spResource::vfunc_14(destination, manager))
        {
            return false;
        }
        target->SetCommonStateForAnalysis(
            width_, height_, pixelFormat_, initialized_);
        target->backingTexture_ = backingTexture_;
        return true;
    }

    const spRTTIRecord& spRenderTarget::vfunc_18() const noexcept
    {
        return RenderTargetRecord;
    }

    bool spRenderTarget::ReinitTargetsForDeviceReset()
    {
        if (width_ == 0 || height_ == 0
            || pixelFormat_ == eTBPixelFormat::Unknown6)
        {
            return false;
        }
        ReleaseTargetsForDeviceReset();
        return Init(width_, height_, pixelFormat_);
    }

    void spRenderTarget::ReleaseTargetsForDeviceReset() noexcept
    {
        initialized_ = false;
    }

    std::uint32_t spRenderTarget::GetWidthForAnalysis() const noexcept
    {
        return width_;
    }

    std::uint32_t spRenderTarget::GetHeightForAnalysis() const noexcept
    {
        return height_;
    }

    eTBPixelFormat spRenderTarget::GetPixelFormatForAnalysis() const noexcept
    {
        return pixelFormat_;
    }

    bool spRenderTarget::IsInitializedForAnalysis() const noexcept
    {
        return initialized_;
    }

    bool spRenderTarget::HasBackendTargetForAnalysis() const noexcept
    {
        return initialized_;
    }

    spTexture* spRenderTarget::GetBackingTextureForAnalysis() const noexcept
    {
        return backingTexture_;
    }

    void spRenderTarget::SetBackingTextureForAnalysis(spTexture* texture) noexcept
    {
        backingTexture_ = texture;
    }

    void spRenderTarget::SetCommonStateForAnalysis(
        const std::uint32_t width,
        const std::uint32_t height,
        const eTBPixelFormat pixelFormat,
        const bool initialized) noexcept
    {
        width_ = width;
        height_ = height;
        pixelFormat_ = pixelFormat;
        initialized_ = initialized;
    }
}
