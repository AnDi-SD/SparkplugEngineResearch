#include "spCameraDataSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateCameraDataSerializer()
        {
            return std::make_unique<spCameraDataSerializer>();
        }

        const spRTTIRecord CameraDataSerializerRecord{
            spCameraDataSerializer::ClassID,
            spCameraSerializer::ClassID,
            "spCameraDataSerializer",
            &spCameraSerializer::StaticRTTI(),
            &CreateCameraDataSerializer,
            nullptr,
        };

        const bool CameraDataSerializerRegistered =
            spRTTIManager::Instance().Register(CameraDataSerializerRecord);
    }

    spCameraDataSerializer::~spCameraDataSerializer() = default;

    const spRTTIRecord& spCameraDataSerializer::StaticRTTI() noexcept
    {
        (void)CameraDataSerializerRegistered;
        return CameraDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spCameraDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spCameraDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spCameraDataSerializer::vfunc_18() const noexcept
    {
        return CameraDataSerializerRecord;
    }

    spClassID spCameraDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }
}
