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
            spRTTIManager::Instance().Register(PCFontManagerRecord);
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
        if (!spFontManager::InitializeForAnalysis())
        {
            return false;
        }
        // PC 0x004C3830 allocates a platform font buffer only after the common
        // initializer succeeds.  The D3D-backed allocation is represented by
        // state until its owner class has been reconstructed.
        platformBufferReady_ = true;
        return true;
    }

    bool spPCFontManager::IsPlatformBufferReadyForAnalysis() const noexcept
    {
        return platformBufferReady_;
    }
}
