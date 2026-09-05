#include "spPS2MeshDataSerializer.h"

#include "spPS2MeshData.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2MeshDataSerializer()
        {
            return std::make_unique<spPS2MeshDataSerializer>();
        }

        const spRTTIRecord PS2MeshDataSerializerRecord{
            spPS2MeshDataSerializer::ClassID,
            spMeshDataSerializer::ClassID,
            "spPS2MeshDataSerializer",
            &spMeshDataSerializer::StaticRTTI(),
            &CreatePS2MeshDataSerializer,
            nullptr,
        };

        const bool PS2MeshDataSerializerRegistered =
            spRTTIManager::Instance().Register(PS2MeshDataSerializerRecord);
    }

    spPS2MeshDataSerializer::~spPS2MeshDataSerializer() = default;

    const spRTTIRecord& spPS2MeshDataSerializer::StaticRTTI() noexcept
    {
        (void)PS2MeshDataSerializerRegistered;
        return PS2MeshDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spPS2MeshDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2MeshDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spPS2MeshDataSerializer::vfunc_18() const noexcept
    {
        return PS2MeshDataSerializerRecord;
    }

    spClassID spPS2MeshDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spPS2MeshData::ClassID;
    }

    std::vector<spPS2MeshDataSerializer::Field>
    spPS2MeshDataSerializer::BuildKnownWritePlanForAnalysis(
        const std::uint32_t nativeSerializationMode) const
    {
        std::vector<Field> plan;
        if (EmitsCrossPlatformPayloadForAnalysis(nativeSerializationMode))
        {
            plan.push_back(Field::CrossPlatform);
        }
        plan.push_back(Field::PlatformSpecific);
        plan.push_back(Field::BoundingBox);
        return plan;
    }

    bool spPS2MeshDataSerializer::PCLoadsNativePayloadForAnalysis(
        const std::uint32_t readerFlags) noexcept
    {
        return (readerFlags & PCNativeLoadFlagMask) != 0;
    }

    bool spPS2MeshDataSerializer::PS2LoadsNativePayloadForAnalysis(
        const std::uint32_t readerFlags) noexcept
    {
        return (readerFlags & PS2NativeLoadFlagMask) != 0;
    }
}
