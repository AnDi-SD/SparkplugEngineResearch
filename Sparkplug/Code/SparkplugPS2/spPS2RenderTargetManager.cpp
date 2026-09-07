#include "spPS2RenderTargetManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2RenderTargetManager()
        {
            return std::make_unique<spPS2RenderTargetManager>();
        }

        const spRTTIRecord PS2RenderTargetManagerRecord{
            spPS2RenderTargetManager::ClassID,
            spRenderTargetManager::ClassID,
            "spPS2RenderTargetManager",
            &spRenderTargetManager::StaticRTTI(),
            &CreatePS2RenderTargetManager,
            nullptr,
        };

        const bool PS2RenderTargetManagerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(PS2RenderTargetManagerRecord);
    }

    const spRTTIRecord& spPS2RenderTargetManager::StaticRTTI() noexcept
    {
        (void)PS2RenderTargetManagerRegistered;
        return PS2RenderTargetManagerRecord;
    }

    std::unique_ptr<spBaseObject> spPS2RenderTargetManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2RenderTargetManager>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    const spRTTIRecord& spPS2RenderTargetManager::vfunc_18() const noexcept
    {
        return PS2RenderTargetManagerRecord;
    }
}
