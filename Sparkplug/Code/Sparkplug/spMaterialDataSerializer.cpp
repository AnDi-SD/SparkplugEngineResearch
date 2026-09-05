#include "spMaterialDataSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMaterialDataSerializer()
        {
            return std::make_unique<spMaterialDataSerializer>();
        }

        const spRTTIRecord MaterialDataSerializerRecord{
            spMaterialDataSerializer::ClassID,
            spMaterialSerializer::ClassID,
            "spMaterialDataSerializer",
            &spMaterialSerializer::StaticRTTI(),
            &CreateMaterialDataSerializer,
            nullptr,
        };

        const bool MaterialDataSerializerRegistered =
            spRTTIManager::Instance().Register(MaterialDataSerializerRecord);
    }

    spMaterialDataSerializer::~spMaterialDataSerializer() = default;

    const spRTTIRecord& spMaterialDataSerializer::StaticRTTI() noexcept
    {
        (void)MaterialDataSerializerRegistered;
        return MaterialDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spMaterialDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMaterialDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spMaterialDataSerializer::vfunc_18() const noexcept
    {
        return MaterialDataSerializerRecord;
    }

    spClassID spMaterialDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    bool spMaterialDataSerializer::CanReadIntoObjectForAnalysis(
        const bool hasTargetObject) noexcept
    {
        return hasTargetObject;
    }
}
