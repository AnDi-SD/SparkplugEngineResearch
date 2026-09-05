#include "spPS2InputManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2InputManager()
        {
            return std::make_unique<spPS2InputManager>();
        }

        const spRTTIRecord PS2InputManagerRecord{
            spPS2InputManager::ClassID,
            spInputManager::ClassID,
            "spPS2InputManager",
            &spInputManager::StaticRTTI(),
            &CreatePS2InputManager,
            nullptr,
        };

        const bool PS2InputManagerRegistered =
            spRTTIManager::Instance().Register(PS2InputManagerRecord);
    }

    spPS2InputManager::~spPS2InputManager()
    {
        ShutdownForAnalysis();
    }

    const spRTTIRecord& spPS2InputManager::StaticRTTI() noexcept
    {
        (void)PS2InputManagerRegistered;
        return PS2InputManagerRecord;
    }

    std::unique_ptr<spBaseObject> spPS2InputManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2InputManager>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spPS2InputManager::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        return destination.IsKindOf(ClassID)
            && spCrossPlatform::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spPS2InputManager::vfunc_18() const noexcept
    {
        return PS2InputManagerRecord;
    }

    bool spPS2InputManager::InitializeForAnalysis()
    {
        connected_.fill(false);
        SetInitializedForAnalysis(true);
        return true;
    }

    void spPS2InputManager::ShutdownForAnalysis() noexcept
    {
        connected_.fill(false);
        SetInitializedForAnalysis(false);
    }

    std::size_t spPS2InputManager::GetMaximumControllerCountForAnalysis()
        const noexcept
    {
        return ControllerCapacity;
    }

    bool spPS2InputManager::IsControllerConnectedForAnalysis(
        const std::size_t index) const noexcept
    {
        return index < connected_.size() && connected_[index];
    }

    bool spPS2InputManager::SetControllerConnectedForAnalysis(
        const std::size_t index,
        const bool connected) noexcept
    {
        if (index >= connected_.size())
        {
            return false;
        }
        connected_[index] = connected;
        return true;
    }
}
