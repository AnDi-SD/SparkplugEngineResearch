#include "spSerializerHook.h"

#include "spSerializerManager.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2SerializerHook()
        {
            return std::make_unique<spPS2SerializerHook>();
        }

        const spRTTIRecord SerializerHookRecord{
            spSerializerHook::ClassID,
            spBaseObject::ClassID,
            "spSerializerHook",
            &spBaseObject::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const spRTTIRecord PS2SerializerHookRecord{
            spPS2SerializerHook::ClassID,
            spSerializerHook::ClassID,
            "spPS2SerializerHook",
            &SerializerHookRecord,
            &CreatePS2SerializerHook,
            nullptr,
        };

        const bool SerializerHookRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(SerializerHookRecord);
        const bool PS2SerializerHookRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(PS2SerializerHookRecord);

        // PS2 sub_00208E70 allocates a manager if its singleton is null and
        // leaves it process-owned. A static unique_ptr preserves that lifetime
        // without reproducing the native raw-pointer leak. It is only used on
        // the fallback edge; the ordinary load path already owns a manager.
        std::unique_ptr<spSerializerManager> HookOwnedSerializerManager;
    }

    spSerializerHook::~spSerializerHook() = default;

    const spRTTIRecord& spSerializerHook::StaticRTTI() noexcept
    {
        (void)SerializerHookRegistered;
        return SerializerHookRecord;
    }

    std::unique_ptr<spBaseObject> spSerializerHook::vfunc_10(
        spCloneManager&) const
    {
        return nullptr;
    }

    const spRTTIRecord& spSerializerHook::vfunc_18() const noexcept
    {
        return SerializerHookRecord;
    }

    spPS2SerializerHook::~spPS2SerializerHook() = default;

    const spRTTIRecord& spPS2SerializerHook::StaticRTTI() noexcept
    {
        (void)PS2SerializerHookRegistered;
        return PS2SerializerHookRecord;
    }

    std::unique_ptr<spBaseObject> spPS2SerializerHook::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2SerializerHook>();
        manager.RegisterClone(*this, *clone);
        return spBaseObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spPS2SerializerHook::vfunc_18() const noexcept
    {
        return PS2SerializerHookRecord;
    }

    void spPS2SerializerHook::vfunc_24(
        spResourceFATHelperForAnalysis* fatHelper,
        spStream& source)
    {
        // The native body does not dereference either argument. Both branches
        // after testing manager.platformMask == 8 converge directly on return.
        (void)fatHelper;
        (void)source;
        if (spSerializerManager::GetInstance() == nullptr)
        {
            HookOwnedSerializerManager =
                std::make_unique<spSerializerManager>();
        }
    }
}
