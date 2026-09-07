#include "spPS2FontManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2FontManager()
        {
            return std::make_unique<spPS2FontManager>();
        }

        const spRTTIRecord PS2FontManagerRecord{
            spPS2FontManager::ClassID,
            spFontManager::ClassID,
            "spPS2FontManager",
            &spFontManager::StaticRTTI(),
            &CreatePS2FontManager,
            nullptr,
        };

        const bool PS2FontManagerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(PS2FontManagerRecord);
    }

    const spRTTIRecord& spPS2FontManager::StaticRTTI() noexcept
    {
        (void)PS2FontManagerRegistered;
        return PS2FontManagerRecord;
    }

    std::unique_ptr<spBaseObject> spPS2FontManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2FontManager>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spPS2FontManager::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        return destination.IsKindOf(ClassID)
            && spCrossPlatform::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spPS2FontManager::vfunc_18() const noexcept
    {
        return PS2FontManagerRecord;
    }
}
