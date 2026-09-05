#include "spPCRenderTargetManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePCRenderTargetManager()
        {
            return std::make_unique<spPCRenderTargetManager>();
        }

        const spRTTIRecord PCRenderTargetManagerRecord{
            spPCRenderTargetManager::ClassID,
            spRenderTargetManager::ClassID,
            "spPCRenderTargetManager",
            &spRenderTargetManager::StaticRTTI(),
            &CreatePCRenderTargetManager,
            nullptr,
        };

        const bool PCRenderTargetManagerRegistered =
            spRTTIManager::Instance().Register(PCRenderTargetManagerRecord);
    }

    const spRTTIRecord& spPCRenderTargetManager::StaticRTTI() noexcept
    {
        (void)PCRenderTargetManagerRegistered;
        return PCRenderTargetManagerRecord;
    }

    std::unique_ptr<spBaseObject> spPCRenderTargetManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPCRenderTargetManager>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    const spRTTIRecord& spPCRenderTargetManager::vfunc_18() const noexcept
    {
        return PCRenderTargetManagerRecord;
    }
}
