#include "spPCAsyncFileStreamManager.h"

#include "../SparkBase/spMemoryStream.h"
#include "spPCFileStream.h"

#include <memory>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePCAsyncFileStreamManager()
        {
            return std::make_unique<spPCAsyncFileStreamManager>();
        }

        const spRTTIRecord PCAsyncFileStreamManagerRecord{
            spPCAsyncFileStreamManager::ClassID,
            spAsyncFileStreamManager::ClassID,
            "spPCAsyncFileStreamManager",
            &spAsyncFileStreamManager::StaticRTTI(),
            &CreatePCAsyncFileStreamManager,
            nullptr,
        };
    }

    const spRTTIRecord& spPCAsyncFileStreamManager::StaticRTTI() noexcept
    {
        return PCAsyncFileStreamManagerRecord;
    }

    std::unique_ptr<spBaseObject> spPCAsyncFileStreamManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPCAsyncFileStreamManager>();
        manager.RegisterClone(*this, *clone);
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    const spRTTIRecord& spPCAsyncFileStreamManager::vfunc_18() const noexcept
    {
        return PCAsyncFileStreamManagerRecord;
    }

    bool spPCAsyncFileStreamManager::vfunc_Request(
        const char* streamName,
        spMemoryStream* destination,
        const CompletionCallback completion,
        void* context)
    {
        // PC 0x006BD400 performs this entire operation synchronously and
        // deliberately ignores every stream result.  Native callers satisfy
        // the non-null destination/callback precondition.
        auto source = std::make_unique<spPCFileStream>();
        (void)source->Open(streamName);

        std::uint32_t size = 0;
        (void)source->GetSize(&size);
        (void)destination->ResizeAndSetSize(size);
        (void)destination->vfunc_WriteFromStream(source.get(), size);
        completion(context);
        (void)source->Close();
        return true;
    }
}
