#include "spPS2MaterialDataSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2MaterialDataSerializer()
        {
            return std::make_unique<spPS2MaterialDataSerializer>();
        }

        const spRTTIRecord PS2MaterialDataSerializerRecord{
            spPS2MaterialDataSerializer::ClassID,
            spMaterialSerializer::ClassID,
            "spPS2MaterialDataSerializer",
            &spMaterialSerializer::StaticRTTI(),
            &CreatePS2MaterialDataSerializer,
            nullptr,
        };

        const bool PS2MaterialDataSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(PS2MaterialDataSerializerRecord);
    }

    spPS2MaterialDataSerializer::~spPS2MaterialDataSerializer() = default;

    const spRTTIRecord& spPS2MaterialDataSerializer::StaticRTTI() noexcept
    {
        (void)PS2MaterialDataSerializerRegistered;
        return PS2MaterialDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spPS2MaterialDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2MaterialDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spPS2MaterialDataSerializer::vfunc_18() const noexcept
    {
        return PS2MaterialDataSerializerRecord;
    }
}
