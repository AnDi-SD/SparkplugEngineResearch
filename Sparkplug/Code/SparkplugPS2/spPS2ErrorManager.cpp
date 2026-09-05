#include "spPS2ErrorManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2ErrorManager()
        {
            return std::make_unique<spPS2ErrorManager>();
        }

        const spRTTIRecord PS2ErrorManagerRecord{
            spPS2ErrorManager::ClassID,
            spErrorManager::ClassID,
            "spPS2ErrorManager",
            &spErrorManager::StaticRTTI(),
            &CreatePS2ErrorManager,
            nullptr,
        };
    }

    spPS2ErrorManager::spPS2ErrorManager()
        : spErrorManager(Ps2DataCapacity)
    {
    }

    const spRTTIRecord& spPS2ErrorManager::StaticRTTI() noexcept
    {
        return PS2ErrorManagerRecord;
    }

    std::unique_ptr<spBaseObject> spPS2ErrorManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2ErrorManager>();
        manager.RegisterClone(*this, *clone);
        if (!spBaseObject::vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    const spRTTIRecord& spPS2ErrorManager::vfunc_18() const noexcept
    {
        return PS2ErrorManagerRecord;
    }

    spErrorManager::AnalysisHandlerBinding
        spPS2ErrorManager::ResolveHandlerForAnalysis() const noexcept
    {
        // Provider 0x0020DA30 returns 0x0020DA50, an exact two-instruction
        // no-op handler in the retail PS2 executable.
        return {&NativeNoOpHandler, nullptr};
    }

    void spPS2ErrorManager::NativeNoOpHandler(
        const AnalysisDispatch&,
        void*) noexcept
    {
    }
}
