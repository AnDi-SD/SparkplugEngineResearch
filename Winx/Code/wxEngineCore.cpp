#include "wxEngineCore.h"

namespace winx::reconstruction
{
    using sparkplug::reconstruction::spBaseObject;
    using sparkplug::reconstruction::spCloneManager;
    using sparkplug::reconstruction::spEngineCore;
    using sparkplug::reconstruction::spRTTIManager;
    using sparkplug::reconstruction::spRTTIRecord;

    namespace
    {
        std::unique_ptr<spBaseObject> CreateEngineCore()
        {
            return std::make_unique<wxEngineCore>();
        }

        const spRTTIRecord EngineCoreRecord{
            wxEngineCore::ClassID,
            spEngineCore::ClassID,
            "wxEngineCore",
            &spEngineCore::StaticRTTI(),
            &CreateEngineCore,
            nullptr,
        };

        const bool EngineCoreRegistered =
            spRTTIManager::Instance().Register(EngineCoreRecord);
    }

    wxEngineCore::~wxEngineCore() = default;

    const spRTTIRecord& wxEngineCore::StaticRTTI() noexcept
    {
        (void)EngineCoreRegistered;
        return EngineCoreRecord;
    }

    std::unique_ptr<spBaseObject> wxEngineCore::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxEngineCore>();
        // Native generic clone construction invokes the inherited empty base
        // copy.  There is no wxEngineCore instance state to transfer.
        if (!spBaseObject::vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    const spRTTIRecord& wxEngineCore::vfunc_18() const noexcept
    {
        return EngineCoreRecord;
    }

    bool wxEngineCore::RunFrameBoundaryForAnalysis(
        const AnalysisActionBinding* const actions,
        const std::size_t actionCount) const
    {
        if (actions == nullptr && actionCount != 0)
        {
            return false;
        }

        for (std::size_t index = 0; index < actionCount; ++index)
        {
            if (actions[index].action != nullptr)
            {
                actions[index].action(actions[index].context);
            }
        }

        // PC 0x00573A50 and PS2 0x00285200 both delegate to the exact base
        // implementation after game-specific work and return its result.
        return InvokeSecondFrameCallbackForAnalysis();
    }
}
