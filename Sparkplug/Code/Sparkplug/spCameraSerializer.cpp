#include "spCameraSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateCameraSerializer()
        {
            return std::make_unique<spCameraSerializer>();
        }

        const spRTTIRecord CameraSerializerRecord{
            spCameraSerializer::ClassID,
            spNodeSerializer::ClassID,
            "spCameraSerializer",
            &spNodeSerializer::StaticRTTI(),
            &CreateCameraSerializer,
            nullptr,
        };

        const bool CameraSerializerRegistered =
            spRTTIManager::Instance().Register(CameraSerializerRecord);
    }

    spCameraSerializer::~spCameraSerializer() = default;

    const spRTTIRecord& spCameraSerializer::StaticRTTI() noexcept
    {
        (void)CameraSerializerRegistered;
        return CameraSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spCameraSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spCameraSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spCameraSerializer::vfunc_18() const noexcept
    {
        return CameraSerializerRecord;
    }

    spClassID spCameraSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spCameraSerializer::Field>
    spCameraSerializer::BuildWritePlanForAnalysis(
        const bool isTwoDimensional)
    {
        std::vector<Field> plan{Field::Camera};
        if (isTwoDimensional)
        {
            plan.push_back(Field::TwoDimensional);
        }
        return plan;
    }

    bool spCameraSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID <= static_cast<std::uint32_t>(Field::TwoDimensional);
    }
}
