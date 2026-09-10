#include "spRenderer.h"
#include "spRenderNode.h"
#include "spRenderable.h"
#include "spMaterialPassLayer.h"
#include "spMaterialTexture.h"
#include "spStdLayer.h"
#include "../SparkplugDX/spDXMaterial.h"

#include <algorithm>
#include <limits>
#include <utility>

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
            spRTTIManager::Instance().RegisterDeferredForAnalysis(RendererRecord);
    }

    spRenderer* spRenderer::instance_ = nullptr;

    std::unique_ptr<spDXMaterial> spRenderer::CreatePCDefaultMaterialForAnalysis()
    {
        // Actual constructor after its natural protected preparation: create
        // DXMaterial, append one pass, append two StdLayers. The sole changed
        // leaf default is raw texture state1 of the second layer (13B8991).
        auto material=std::make_unique<spDXMaterial>();
        auto pass=std::make_shared<spMaterialPassLayer>();
        if(!material->SetPassForAnalysis(material->GetPassCountForAnalysis(),pass)
            ||!pass->SetLayerForAnalysis(pass->GetLayerCountForAnalysis(),std::make_unique<spStdLayer>()))return nullptr;
        auto second=std::make_unique<spStdLayer>();
        second->GetMaterialTextureForAnalysis()->SetTextureStateForAnalysis(1,0);
        if(!pass->SetLayerForAnalysis(pass->GetLayerCountForAnalysis(),std::move(second)))return nullptr;
        return material;
    }

    bool spRenderer::EnqueueAlphaForAnalysis(AlphaQueueForAnalysis& state,
        spRenderable* object,spRenderNode* support,const AlphaCameraInputForAnalysis* camera,
        std::uint32_t priority) noexcept
    {
        if(!state.enabled)return true;
        if(!object||!support||!camera||!camera->identity)return false; // host invalid-pointer guard
        const auto& sphere=object->GetBoundingSphereForAnalysis();
        auto key=evidence::pc::renderer_queue_math::BuildAlphaKey({sphere[0],sphere[1],sphere[2]},
            support->GetCachedRenderMatrixForAnalysis(),camera->view,camera->depthOnly,priority,state.priorityBase,false);
        if(state.count>=AlphaQueueForAnalysis::Capacity)return false;
        key.exactParticleSystem=object->IsExactly(0x5AFA1A4F); // exact spParticleSystem, not IsKindOf
        state.entries[state.count++]={camera->identity,support,object,key};return true;
    }

    bool spRenderer::FlushAlphaForAnalysis(AlphaQueueForAnalysis& state,const AlphaDispatchForAnalysis& dispatch)
    {
        if(state.dispatching||state.count>AlphaQueueForAnalysis::Capacity||!dispatch.sort)return false;
        state.dispatching=true;
        struct Reset final{bool& active;~Reset(){active=false;}} reset{state.dispatching};
        dispatch.sort(dispatch.context,state.entries.data(),state.count,
            [](const AlphaEntryForAnalysis& a,const AlphaEntryForAnalysis& b)noexcept
            {return evidence::pc::renderer_queue_math::CompareAlpha(a.key,b.key);});
        state.flushing=true;spRenderNode* previous=nullptr;
        for(std::size_t i=0;i<state.count;++i)
        {
            if(state.count>AlphaQueueForAnalysis::Capacity)return false; // host corrupted-count guard
            auto& entry=state.entries[i];
            if(previous!=entry.support)
            {
                if(!entry.support||!dispatch.prepare)return false;
                (void)dispatch.prepare(dispatch.context,*entry.support);
                previous=entry.support; // original rereads it after virtual callback
            }
            if(!entry.renderable||!dispatch.render)return false;
            (void)dispatch.render(dispatch.context,*entry.renderable,entry.camera,entry.support);
        }
        state.flushing=false;state.count=0;return true;
    }

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
        case spRendererPlatformOperationForAnalysis::SetUVTransform3x3:
            return platform == spRendererPlatformForAnalysis::PC ? 24u : 23u;
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
