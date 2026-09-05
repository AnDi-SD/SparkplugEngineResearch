#include "spFogSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateFogSerializer()
        {
            return std::make_unique<spFogSerializer>();
        }

        const spRTTIRecord FogSerializerRecord{
            spFogSerializer::ClassID,
            spSerializer::ClassID,
            "spFogSerializer",
            &spSerializer::StaticRTTI(),
            &CreateFogSerializer,
            nullptr,
        };

        const bool FogSerializerRegistered =
            spRTTIManager::Instance().Register(FogSerializerRecord);
    }

    bool spFogSerializer::FogPayload::operator==(
        const FogPayload& other) const noexcept
    {
        return type == other.type
            && colorARGB == other.colorARGB
            && start == other.start
            && end == other.end
            && density == other.density;
    }

    spFogSerializer::~spFogSerializer() = default;

    const spRTTIRecord& spFogSerializer::StaticRTTI() noexcept
    {
        (void)FogSerializerRegistered;
        return FogSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spFogSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spFogSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spFogSerializer::vfunc_18() const noexcept
    {
        return FogSerializerRecord;
    }

    spClassID spFogSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spFogSerializer::Field>
    spFogSerializer::BuildWritePlanForAnalysis()
    {
        return {Field::Fog};
    }

    bool spFogSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID == static_cast<std::uint32_t>(Field::Fog);
    }
}
