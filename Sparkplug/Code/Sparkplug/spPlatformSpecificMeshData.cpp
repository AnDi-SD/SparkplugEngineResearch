#include "spPlatformSpecificMeshData.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePlatformSpecificMeshData()
        {
            return std::make_unique<spPlatformSpecificMeshData>();
        }

        const spRTTIRecord PlatformSpecificMeshDataRecord{
            spPlatformSpecificMeshData::ClassID,
            spNamedObject::ClassID,
            "spPlatformSpecificMeshData",
            &spNamedObject::StaticRTTI(),
            &CreatePlatformSpecificMeshData,
            nullptr,
        };

        const bool PlatformSpecificMeshDataRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(PlatformSpecificMeshDataRecord);
    }

    spPlatformSpecificMeshData::~spPlatformSpecificMeshData() = default;

    const spRTTIRecord& spPlatformSpecificMeshData::StaticRTTI() noexcept
    {
        (void)PlatformSpecificMeshDataRegistered;
        return PlatformSpecificMeshDataRecord;
    }

    std::unique_ptr<spBaseObject> spPlatformSpecificMeshData::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPlatformSpecificMeshData>();
        manager.RegisterClone(*this, *clone);
        return spNamedObject::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spPlatformSpecificMeshData::vfunc_18() const noexcept
    {
        return PlatformSpecificMeshDataRecord;
    }
}
