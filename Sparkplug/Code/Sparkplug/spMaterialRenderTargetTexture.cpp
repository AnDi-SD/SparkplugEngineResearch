#include "spMaterialRenderTargetTexture.h"

#include "spRenderTargetManager.h"

#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord MaterialRenderTargetTextureRecord{
            spMaterialRenderTargetTexture::ClassID,
            spMaterialTexture::ClassID,
            "spMaterialRenderTargetTexture",
            &spMaterialTexture::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool MaterialRenderTargetTextureRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(MaterialRenderTargetTextureRecord);
    }

    spMaterialRenderTargetTexture::spMaterialRenderTargetTexture()
    {
        auto* const manager = spRenderTargetManager::GetInstance();
        const auto slotCount = manager == nullptr
            ? std::size_t{1}
            : manager->GetTargetSlotCountForAnalysis();
        renderTargets_.resize(slotCount, nullptr);
        if (manager != nullptr)
        {
            (void)manager->RegisterLayerTargetForAnalysis(*this);
        }
    }

    spMaterialRenderTargetTexture::~spMaterialRenderTargetTexture()
    {
        if (auto* const manager = spRenderTargetManager::GetInstance();
            manager != nullptr)
        {
            (void)manager->DeactivateLayerTargetForAnalysis(*this);
        }
        renderTargets_.clear();
    }

    const spRTTIRecord& spMaterialRenderTargetTexture::StaticRTTI() noexcept
    {
        (void)MaterialRenderTargetTextureRegistered;
        return MaterialRenderTargetTextureRecord;
    }

    std::unique_ptr<spBaseObject> spMaterialRenderTargetTexture::vfunc_10(
        spCloneManager&) const
    {
        return nullptr;
    }

    bool spMaterialRenderTargetTexture::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* const target =
            dynamic_cast<spMaterialRenderTargetTexture*>(&destination);
        if (target == nullptr
            || !spMaterialTexture::vfunc_14(destination, manager))
        {
            return false;
        }
        target->maxRecursionLevel_ = maxRecursionLevel_;
        target->currentRecursionLevel_ = 0;
        target->width_ = width_;
        target->height_ = height_;
        target->pixelFormat_ = pixelFormat_;
        std::fill(target->renderTargets_.begin(),
            target->renderTargets_.end(), nullptr);
        return true;
    }

    const spRTTIRecord& spMaterialRenderTargetTexture::vfunc_18()
        const noexcept
    {
        return MaterialRenderTargetTextureRecord;
    }

    spTexture* spMaterialRenderTargetTexture::GetTextureForAnalysis()
        const noexcept
    {
        const auto* const manager = spRenderTargetManager::GetInstance();
        if (manager == nullptr)
        {
            return GetFallBackTextureForAnalysis();
        }
        const auto slot = manager->GetCurrentTargetIndexForAnalysis();
        if (slot == manager->GetTargetSlotCountForAnalysis()
            || slot >= renderTargets_.size()
            || renderTargets_[slot] == nullptr)
        {
            return GetFallBackTextureForAnalysis();
        }
        auto* const texture = renderTargets_[slot]->GetBackingTextureForAnalysis();
        return texture == nullptr ? GetFallBackTextureForAnalysis() : texture;
    }

    std::uint32_t
    spMaterialRenderTargetTexture::GetMaxRecursionLevelForAnalysis()
        const noexcept
    {
        return maxRecursionLevel_;
    }

    std::uint32_t
    spMaterialRenderTargetTexture::GetCurrentRecursionLevelForAnalysis()
        const noexcept
    {
        return currentRecursionLevel_;
    }

    std::uint32_t spMaterialRenderTargetTexture::GetWidthForAnalysis()
        const noexcept
    {
        return width_;
    }

    std::uint32_t spMaterialRenderTargetTexture::GetHeightForAnalysis()
        const noexcept
    {
        return height_;
    }

    eTBPixelFormat spMaterialRenderTargetTexture::GetPixelFormatForAnalysis()
        const noexcept
    {
        return pixelFormat_;
    }

    std::size_t
    spMaterialRenderTargetTexture::GetRenderTargetSlotCountForAnalysis()
        const noexcept
    {
        return renderTargets_.size();
    }

    void spMaterialRenderTargetTexture::SetMaxRecursionLevelForAnalysis(
        const std::uint32_t level) noexcept
    {
        maxRecursionLevel_ = level;
    }

    void spMaterialRenderTargetTexture::SetDimensionsForAnalysis(
        const std::uint32_t width,
        const std::uint32_t height) noexcept
    {
        width_ = width;
        height_ = height;
    }

    void spMaterialRenderTargetTexture::SetPixelFormatForAnalysis(
        const eTBPixelFormat format) noexcept
    {
        pixelFormat_ = format;
    }

    bool spMaterialRenderTargetTexture::SetRenderTargetForAnalysis(
        const std::size_t slot,
        spRenderTarget* target) noexcept
    {
        if (slot >= renderTargets_.size())
        {
            return false;
        }
        renderTargets_[slot] = target;
        return true;
    }

    bool spMaterialRenderTargetTexture::TryEnterRenderForAnalysis() noexcept
    {
        if (currentRecursionLevel_ >= maxRecursionLevel_)
        {
            return false;
        }
        ++currentRecursionLevel_;
        return true;
    }

    void spMaterialRenderTargetTexture::LeaveRenderForAnalysis() noexcept
    {
        if (currentRecursionLevel_ != 0)
        {
            --currentRecursionLevel_;
        }
    }

    void spMaterialRenderTargetTexture::
        ReleaseRenderTargetsForDeviceResetForAnalysis() noexcept
    {
        for (auto* const target : renderTargets_)
        {
            if (target != nullptr)
            {
                target->ReleaseTargetsForDeviceReset();
            }
        }
    }

    bool spMaterialRenderTargetTexture::
        ReinitRenderTargetsForDeviceResetForAnalysis()
    {
        bool success = true;
        for (auto* const target : renderTargets_)
        {
            if (target != nullptr)
            {
                success = target->ReinitTargetsForDeviceReset() && success;
            }
        }
        return success;
    }
}
