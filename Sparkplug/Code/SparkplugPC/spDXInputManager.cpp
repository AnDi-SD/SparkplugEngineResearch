#include "spDXInputManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXInputManager()
        {
            return std::make_unique<spDXInputManager>();
        }

        const spRTTIRecord DXInputManagerRecord{
            spDXInputManager::ClassID,
            spInputManager::ClassID,
            "spDXInputManager",
            &spInputManager::StaticRTTI(),
            &CreateDXInputManager,
            nullptr,
        };

        const bool DXInputManagerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXInputManagerRecord);
    }

    spDXInputManager::~spDXInputManager()
    {
        ShutdownForAnalysis();
    }

    const spRTTIRecord& spDXInputManager::StaticRTTI() noexcept
    {
        (void)DXInputManagerRegistered;
        return DXInputManagerRecord;
    }

    std::unique_ptr<spBaseObject> spDXInputManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXInputManager>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spDXInputManager::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        return destination.IsKindOf(ClassID)
            && spCrossPlatform::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spDXInputManager::vfunc_18() const noexcept
    {
        return DXInputManagerRecord;
    }

    bool spDXInputManager::InitializeForAnalysis()
    {
        connected_.fill(false);
        SetInitializedForAnalysis(true);
        return true;
    }

    void spDXInputManager::ShutdownForAnalysis() noexcept
    {
        connected_.fill(false);
        SetInitializedForAnalysis(false);
    }

    std::size_t spDXInputManager::GetMaximumControllerCountForAnalysis()
        const noexcept
    {
        return ControllerCapacity;
    }

    bool spDXInputManager::IsControllerConnectedForAnalysis(
        const std::size_t index) const noexcept
    {
        return index < connected_.size() && connected_[index];
    }

    bool spDXInputManager::SetControllerConnectedForAnalysis(
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
