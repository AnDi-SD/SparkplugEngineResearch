#include "spInputManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord InputManagerRecord{
            spInputManager::ClassID,
            spCrossPlatform::ClassID,
            "spInputManager",
            &spCrossPlatform::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool InputManagerRegistered =
            spRTTIManager::Instance().Register(InputManagerRecord);
    }

    spInputManager* spInputManager::instance_ = nullptr;

    spInputManager::spInputManager() noexcept
    {
        instance_ = this;
    }

    spInputManager::~spInputManager()
    {
        initialized_ = false;
        instance_ = nullptr;
    }

    const spRTTIRecord& spInputManager::StaticRTTI() noexcept
    {
        (void)InputManagerRegistered;
        return InputManagerRecord;
    }

    spInputManager* spInputManager::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spInputManager::vfunc_10(spCloneManager&) const
    {
        return nullptr;
    }

    const spRTTIRecord& spInputManager::vfunc_18() const noexcept
    {
        return InputManagerRecord;
    }

    bool spInputManager::IsInitializedForAnalysis() const noexcept
    {
        return initialized_;
    }

    std::size_t spInputManager::GetConnectedControllerCountForAnalysis()
        const noexcept
    {
        std::size_t count = 0;
        for (std::size_t index = 0;
             index < GetMaximumControllerCountForAnalysis(); ++index)
        {
            if (IsControllerConnectedForAnalysis(index))
            {
                ++count;
            }
        }
        return count;
    }

    void spInputManager::SetInitializedForAnalysis(
        const bool initialized) noexcept
    {
        initialized_ = initialized;
    }
}
