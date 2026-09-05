#include "spRenderer.h"

#include <algorithm>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord RendererRecord{
            spRenderer::ClassID,
            spCrossPlatform::ClassID,
            "spRenderer",
            &spCrossPlatform::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool RendererRegistered =
            spRTTIManager::Instance().Register(RendererRecord);
    }

    spRenderer* spRenderer::instance_ = nullptr;

    spRenderer::spRenderer(const std::size_t textureStateCacheCount)
        : renderStateCache_(RenderStateCacheCount),
          textureStateCache_(textureStateCacheCount)
    {
        instance_ = this;
        (void)InvalidateStateCachesForAnalysis();
    }

    spRenderer::~spRenderer()
    {
        instance_ = nullptr;
    }

    const spRTTIRecord& spRenderer::StaticRTTI() noexcept
    {
        (void)RendererRegistered;
        return RendererRecord;
    }

    spRenderer* spRenderer::GetInstance() noexcept
    {
        return instance_;
    }

    std::size_t spRenderer::GetPlatformInterfaceSlotForAnalysis(
        const spRendererPlatformForAnalysis platform,
        const spRendererPlatformOperationForAnalysis operation) noexcept
    {
        switch (operation)
        {
        case spRendererPlatformOperationForAnalysis::BindRenderTarget:
            return platform == spRendererPlatformForAnalysis::PC ? 1u : 0u;
        case spRendererPlatformOperationForAnalysis::BindCubeRenderTarget:
            return platform == spRendererPlatformForAnalysis::PC ? 0u : 1u;
        case spRendererPlatformOperationForAnalysis::BeginScene:
            return 3u;
        case spRendererPlatformOperationForAnalysis::EndScene:
            return 4u;
        case spRendererPlatformOperationForAnalysis::Clear:
            return 5u;
        case spRendererPlatformOperationForAnalysis::SubmitMesh:
            return 9u;
        case spRendererPlatformOperationForAnalysis::Configure2D:
            return 10u;
        case spRendererPlatformOperationForAnalysis::SetProjectionMatrix:
            return 12u;
        case spRendererPlatformOperationForAnalysis::SetViewMatrix:
            return 13u;
        case spRendererPlatformOperationForAnalysis::SetWorldMatrix:
            return 14u;
        case spRendererPlatformOperationForAnalysis::SetViewport:
            return 22u;
        case spRendererPlatformOperationForAnalysis::SetTextureTransform:
            return 23u;
        case spRendererPlatformOperationForAnalysis::SetFog:
            return 26u;
        }
        return PlatformInterfaceSlotCount;
    }

    std::unique_ptr<spBaseObject> spRenderer::vfunc_10(
        spCloneManager&) const
    {
        // Both native common registrations have a null factory, and their
        // clone slot is the shared null-clone implementation.
        return nullptr;
    }

    const spRTTIRecord& spRenderer::vfunc_18() const noexcept
    {
        return RendererRecord;
    }

    bool spRenderer::InvalidateStateCachesForAnalysis() noexcept
    {
        constexpr auto InvalidState =
            std::numeric_limits<std::uint32_t>::max();
        std::fill(renderStateCache_.begin(), renderStateCache_.end(),
            InvalidState);
        std::fill(textureStateCache_.begin(), textureStateCache_.end(),
            InvalidState);
        return true;
    }

    std::size_t spRenderer::GetRenderStateCacheCountForAnalysis()
        const noexcept
    {
        return renderStateCache_.size();
    }

    std::size_t spRenderer::GetTextureStateCacheCountForAnalysis()
        const noexcept
    {
        return textureStateCache_.size();
    }

    std::uint32_t spRenderer::GetRenderStateCacheForAnalysis(
        const std::size_t index) const noexcept
    {
        return index < renderStateCache_.size()
            ? renderStateCache_[index]
            : 0;
    }

    std::uint32_t spRenderer::GetTextureStateCacheForAnalysis(
        const std::size_t index) const noexcept
    {
        return index < textureStateCache_.size()
            ? textureStateCache_[index]
            : 0;
    }
}
