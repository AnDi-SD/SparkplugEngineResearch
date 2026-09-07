#include "spPCRenderer.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePCRenderer()
        {
            return std::make_unique<spPCRenderer>();
        }

        const spRTTIRecord PCRendererRecord{
            spPCRenderer::ClassID,
            spDXRenderer::ClassID,
            "spPCRenderer",
            &spDXRenderer::StaticRTTI(),
            &CreatePCRenderer,
            nullptr,
        };

        const bool PCRendererRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(PCRendererRecord);
    }

    const spRTTIRecord& spPCRenderer::StaticRTTI() noexcept
    {
        (void)PCRendererRegistered;
        return PCRendererRecord;
    }

    std::unique_ptr<spBaseObject> spPCRenderer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPCRenderer>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    const spRTTIRecord& spPCRenderer::vfunc_18() const noexcept
    {
        return PCRendererRecord;
    }
}
