#include "spDebugManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDebugManager()
        {
            return std::make_unique<spDebugManager>();
        }

        const spRTTIRecord DebugManagerRecord{
            spDebugManager::ClassID,
            spBaseObject::ClassID,
            "spDebugManager",
            &spBaseObject::StaticRTTI(),
            &CreateDebugManager,
            nullptr,
        };

        const bool DebugManagerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DebugManagerRecord);
    }

    spDebugManager* spDebugManager::instance_ = nullptr;

    spDebugManager::spDebugManager() noexcept
    {
        instance_ = this;
    }

    spDebugManager::~spDebugManager()
    {
        instance_ = nullptr;
    }

    const spRTTIRecord& spDebugManager::StaticRTTI() noexcept
    {
        (void)DebugManagerRegistered;
        return DebugManagerRecord;
    }

    spDebugManager* spDebugManager::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spDebugManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDebugManager>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spDebugManager::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // Native clone construction invokes the root no-payload copy slot;
        // debug flags, cursor and renderer helpers are runtime-only.
        return destination.IsKindOf(ClassID)
            && spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spDebugManager::vfunc_18() const noexcept
    {
        return DebugManagerRecord;
    }

    bool spDebugManager::SetFlagForAnalysis(
        const std::size_t index,
        const bool enabled) noexcept
    {
        if (index >= flags_.size())
        {
            return false;
        }
        flags_[index] = enabled ? 1U : 0U;
        return true;
    }

    bool spDebugManager::GetFlagForAnalysis(const std::size_t index) const noexcept
    {
        return index < flags_.size() && flags_[index] != 0;
    }

    std::uint32_t spDebugManager::AdvanceCycleIndexForAnalysis() noexcept
    {
        cycleIndex_ = (cycleIndex_ + 1U) % CycleLength;
        return cycleIndex_;
    }

    std::uint32_t spDebugManager::GetCycleIndexForAnalysis() const noexcept
    {
        return cycleIndex_;
    }
}
