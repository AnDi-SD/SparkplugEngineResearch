#include "spCameraData.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateCameraData()
        {
            return std::make_unique<spCameraData>();
        }

        const spRTTIRecord CameraDataRecord{
            spCameraData::ClassID,
            spCamera::ClassID,
            "spCameraData",
            &spCamera::StaticRTTI(),
            &CreateCameraData,
            nullptr,
        };

        const bool CameraDataRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(CameraDataRecord);
    }

    const spRTTIRecord& spCameraData::StaticRTTI() noexcept
    {
        (void)CameraDataRegistered;
        return CameraDataRecord;
    }

    std::unique_ptr<spBaseObject> spCameraData::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spCameraData>();
        manager.RegisterClone(*this, *clone);
        // Native spCameraData inherits the spNode copy slot unchanged. That
        // copies node state but deliberately leaves the new camera defaults.
        return spNode::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spCameraData::vfunc_18() const noexcept
    {
        return CameraDataRecord;
    }
}
