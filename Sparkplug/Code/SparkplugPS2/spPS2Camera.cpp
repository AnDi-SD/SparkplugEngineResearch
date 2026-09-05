#include "spPS2Camera.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2Camera()
        {
            return std::make_unique<spPS2Camera>();
        }

        const spRTTIRecord PS2CameraRecord{
            spPS2Camera::ClassID,
            spCamera::ClassID,
            "spPS2Camera",
            &spCamera::StaticRTTI(),
            &CreatePS2Camera,
            nullptr,
        };

        const bool PS2CameraRegistered =
            spRTTIManager::Instance().Register(PS2CameraRecord);
    }

    const spRTTIRecord& spPS2Camera::StaticRTTI() noexcept
    {
        (void)PS2CameraRegistered;
        return PS2CameraRecord;
    }

    std::unique_ptr<spBaseObject> spPS2Camera::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2Camera>();
        manager.RegisterClone(*this, *clone);
        return spNode::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spPS2Camera::vfunc_18() const noexcept
    {
        return PS2CameraRecord;
    }
}
