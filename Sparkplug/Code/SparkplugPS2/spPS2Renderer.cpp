#include "spPS2Renderer.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2Renderer()
        {
            return std::make_unique<spPS2Renderer>();
        }

        const spRTTIRecord PS2RendererRecord{
            spPS2Renderer::ClassID,
            spRenderer::ClassID,
            "spPS2Renderer",
            &spRenderer::StaticRTTI(),
            &CreatePS2Renderer,
            nullptr,
        };

        const bool PS2RendererRegistered =
            spRTTIManager::Instance().Register(PS2RendererRecord);
    }

    spPS2Renderer::spPS2Renderer()
        : spRenderer(TextureStateCacheCount)
    {
    }

    const spRTTIRecord& spPS2Renderer::StaticRTTI() noexcept
    {
        (void)PS2RendererRegistered;
        return PS2RendererRecord;
    }

    std::unique_ptr<spBaseObject> spPS2Renderer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2Renderer>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    const spRTTIRecord& spPS2Renderer::vfunc_18() const noexcept
    {
        return PS2RendererRecord;
    }
}
