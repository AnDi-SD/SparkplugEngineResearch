#include "spPCRenderTarget.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePCRenderTarget()
        {
            return std::make_unique<spPCRenderTarget>();
        }

        const spRTTIRecord PCRenderTargetRecord{
            spPCRenderTarget::ClassID,
            spDXRenderTarget::ClassID,
            "spPCRenderTarget",
            &spDXRenderTarget::StaticRTTI(),
            &CreatePCRenderTarget,
            nullptr,
        };

        const bool PCRenderTargetRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(PCRenderTargetRecord);
    }

    const spRTTIRecord& spPCRenderTarget::StaticRTTI() noexcept
    {
        (void)PCRenderTargetRegistered;
        return PCRenderTargetRecord;
    }

    std::unique_ptr<spBaseObject> spPCRenderTarget::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPCRenderTarget>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    const spRTTIRecord& spPCRenderTarget::vfunc_18() const noexcept
    {
        return PCRenderTargetRecord;
    }
}
