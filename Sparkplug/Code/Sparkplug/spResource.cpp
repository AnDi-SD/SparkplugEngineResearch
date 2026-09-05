#include "spResource.h"
#include "spResourceManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateResource()
        {
            return std::make_unique<spResource>();
        }

        const spRTTIRecord ResourceRecord{
            spResource::ClassID,
            spNamedObject::ClassID,
            "spResource",
            &spNamedObject::StaticRTTI(),
            &CreateResource,
            nullptr,
        };

        const bool ResourceRegistered =
            spRTTIManager::Instance().Register(ResourceRecord);
    }

    spResource::~spResource()
    {
        if (auto* const manager = spResourceManager::GetInstance();
            manager != nullptr)
        {
            (void)manager->UnregisterForAnalysis(*this);
        }
    }

    const spRTTIRecord& spResource::StaticRTTI() noexcept
    {
        (void)ResourceRegistered;
        return ResourceRecord;
    }

    std::unique_ptr<spBaseObject> spResource::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spResource>();
        manager.RegisterClone(*this, *clone);
        return spNamedObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spResource::vfunc_18() const noexcept
    {
        return ResourceRecord;
    }
}
