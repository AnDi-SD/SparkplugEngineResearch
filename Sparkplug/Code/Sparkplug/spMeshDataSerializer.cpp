#include "spMeshDataSerializer.h"

#include "spMeshData.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMeshDataSerializer()
        {
            return std::make_unique<spMeshDataSerializer>();
        }

        const spRTTIRecord MeshDataSerializerRecord{
            spMeshDataSerializer::ClassID,
            spSerializer::ClassID,
            "spMeshDataSerializer",
            &spSerializer::StaticRTTI(),
            &CreateMeshDataSerializer,
            nullptr,
        };

        const bool MeshDataSerializerRegistered =
            spRTTIManager::Instance().Register(MeshDataSerializerRecord);
    }

    spMeshDataSerializer::~spMeshDataSerializer() = default;

    const spRTTIRecord& spMeshDataSerializer::StaticRTTI() noexcept
    {
        (void)MeshDataSerializerRegistered;
        return MeshDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spMeshDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMeshDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spMeshDataSerializer::vfunc_18() const noexcept
    {
        return MeshDataSerializerRecord;
    }

    spClassID spMeshDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spMeshData::ClassID;
    }

    bool spMeshDataSerializer::EmitsCrossPlatformPayloadForAnalysis(
        const std::uint32_t nativeSerializationMode) noexcept
    {
        return nativeSerializationMode == 0 || nativeSerializationMode == 2;
    }

    std::vector<spMeshDataSerializer::Field>
    spMeshDataSerializer::BuildKnownWritePlanForAnalysis(
        const std::uint32_t nativeSerializationMode) const
    {
        if (EmitsCrossPlatformPayloadForAnalysis(nativeSerializationMode))
        {
            return {Field::CrossPlatform};
        }
        return {};
    }
}
