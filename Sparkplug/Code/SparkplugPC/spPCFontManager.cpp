#include "spPCFontManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePCFontManager()
        {
            return std::make_unique<spPCFontManager>();
        }

        const spRTTIRecord PCFontManagerRecord{
            spPCFontManager::ClassID,
            spFontManager::ClassID,
            "spPCFontManager",
            &spFontManager::StaticRTTI(),
            &CreatePCFontManager,
            nullptr,
        };

        const bool PCFontManagerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(PCFontManagerRecord);
    }

    const spRTTIRecord& spPCFontManager::StaticRTTI() noexcept
    {
        (void)PCFontManagerRegistered;
        return PCFontManagerRecord;
    }

    std::unique_ptr<spBaseObject> spPCFontManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPCFontManager>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spPCFontManager::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // Native clone constructs an empty manager; renderer/font ownership is
        // runtime state and the inherited base-copy slot carries no payload.
        return destination.IsKindOf(ClassID)
            && spCrossPlatform::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spPCFontManager::vfunc_18() const noexcept
    {
        return PCFontManagerRecord;
    }

    bool spPCFontManager::InitializeForAnalysis()
    {
        if (!InitializePCMaterialForAnalysis())
        {
            return false;
        }
        // PC4C3830 next calls4C36A0 to build a system Font and assigns its
        // borrowed pointer28/2C. No modern system-font producer is supplied.
        // Keep the completed material prefix, but do not invent font success.
        platformBufferReady_ = false;
        MarkInitializedForAnalysis(false);
        return false;
    }

    bool spPCFontManager::IsPlatformBufferReadyForAnalysis() const noexcept
    {
        return platformBufferReady_;
    }
}
