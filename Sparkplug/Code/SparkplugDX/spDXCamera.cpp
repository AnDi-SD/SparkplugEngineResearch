#include "spDXCamera.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXCamera()
        {
            return std::make_unique<spDXCamera>();
        }

        const spRTTIRecord DXCameraRecord{
            spDXCamera::ClassID,
            spCamera::ClassID,
            "spDXCamera",
            &spCamera::StaticRTTI(),
            &CreateDXCamera,
            nullptr,
        };

        const bool DXCameraRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXCameraRecord);
    }

    const spRTTIRecord& spDXCamera::StaticRTTI() noexcept
    {
        (void)DXCameraRegistered;
        return DXCameraRecord;
    }

    std::unique_ptr<spBaseObject> spDXCamera::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXCamera>();
        manager.RegisterClone(*this, *clone);
        return spNode::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXCamera::vfunc_18() const noexcept
    {
        return DXCameraRecord;
    }
}
