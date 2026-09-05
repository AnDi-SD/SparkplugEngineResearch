#include "spAsyncFileStreamManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord AsyncFileStreamManagerRecord{
            spAsyncFileStreamManager::ClassID,
            spCrossPlatform::ClassID,
            "spAsyncFileStreamManager",
            &spCrossPlatform::StaticRTTI(),
            nullptr,
            nullptr,
        };
    }

    spAsyncFileStreamManager* spAsyncFileStreamManager::instance_ = nullptr;

    spAsyncFileStreamManager::spAsyncFileStreamManager() noexcept
    {
        // Both native constructors overwrite their platform-global pointer.
        instance_ = this;
    }

    spAsyncFileStreamManager::~spAsyncFileStreamManager()
    {
        // Native destructors clear the global unconditionally rather than
        // checking whether a later instance replaced it.
        instance_ = nullptr;
    }

    const spRTTIRecord& spAsyncFileStreamManager::StaticRTTI() noexcept
    {
        return AsyncFileStreamManagerRecord;
    }

    spAsyncFileStreamManager* spAsyncFileStreamManager::GetInstance() noexcept
    {
        return instance_;
    }

    const spRTTIRecord& spAsyncFileStreamManager::vfunc_18() const noexcept
    {
        return AsyncFileStreamManagerRecord;
    }

    void spAsyncFileStreamManager::vfunc_Update() noexcept
    {
        // PC 0x0048EAA0 is a one-instruction return.  The PS2 base table has
        // the equivalent common slot; its platform leaf supplies real work.
    }
}
