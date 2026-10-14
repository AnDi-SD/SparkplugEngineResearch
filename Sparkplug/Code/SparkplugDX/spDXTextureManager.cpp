#include "spDXTextureManager.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXTextureManager()
        {
            return std::make_unique<spDXTextureManager>();
        }
        const spRTTIRecord DXTextureManagerRecord{
            spDXTextureManager::ClassID, spTextureManager::ClassID,
            "spDXTextureManager", &spTextureManager::StaticRTTI(),
            &CreateDXTextureManager, nullptr,
        };
        const bool DXTextureManagerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXTextureManagerRecord);
    }

    const spRTTIRecord& spDXTextureManager::StaticRTTI() noexcept
    {
        (void)DXTextureManagerRegistered;
        return DXTextureManagerRecord;
    }

    const spRTTIRecord& spDXTextureManager::vfunc_18() const noexcept
    {
        return DXTextureManagerRecord;
    }

    std::unique_ptr<spBaseObject> spDXTextureManager::vfunc_10(
        spCloneManager& manager) const
    {
        // PC005CF170: fresh factory, clone map, then inherited root Copy.
        // Neither configured counts/flags nor opaque records are copied.
        auto clone = std::make_unique<spDXTextureManager>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    void spDXTextureManager::NativeHook1CForAnalysis() noexcept
    {
    }

    void spDXTextureManager::NativeHook20ForAnalysis() noexcept
    {
    }
}
