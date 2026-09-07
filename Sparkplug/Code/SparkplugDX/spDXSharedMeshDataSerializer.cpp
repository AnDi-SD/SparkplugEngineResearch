#include "spDXSharedMeshDataSerializer.h"

#include "spDXCombinedVB.h"
#include "spDXSharedMeshData.h"
#include "../SparkBase/spStream.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXSharedMeshDataSerializer()
        {
            return std::make_unique<spDXSharedMeshDataSerializer>();
        }

        const spRTTIRecord DXSharedMeshDataSerializerRecord{
            spDXSharedMeshDataSerializer::ClassID,
            spSerializer::ClassID,
            "spDXSharedMeshDataSerializer",
            &spSerializer::StaticRTTI(),
            &CreateDXSharedMeshDataSerializer,
            nullptr,
        };

        const bool DXSharedMeshDataSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(
                DXSharedMeshDataSerializerRecord);
    }

    spDXSharedMeshDataSerializer::~spDXSharedMeshDataSerializer() = default;

    const spRTTIRecord& spDXSharedMeshDataSerializer::StaticRTTI() noexcept
    {
        (void)DXSharedMeshDataSerializerRegistered;
        return DXSharedMeshDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject>
    spDXSharedMeshDataSerializer::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXSharedMeshDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord&
    spDXSharedMeshDataSerializer::vfunc_18() const noexcept
    {
        return DXSharedMeshDataSerializerRecord;
    }

    spClassID
    spDXSharedMeshDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    spClassID spDXSharedMeshDataSerializer::ResolveClassIDForAnalysis(
        const spClassID) const noexcept
    {
        return TargetClassID;
    }

    spClassID
    spDXSharedMeshDataSerializer::GetSourceClassIDForAnalysis() const noexcept
    {
        return SourceClassID;
    }

    bool spDXSharedMeshDataSerializer::WritePayloadForAnalysis(
        spStream& destination,
        const spDXCombinedVB& payload) const
    {
        const auto indexSize = payload.GetIndexByteSizeForAnalysis();
        const auto vertexSize = payload.GetVertexByteSizeForAnalysis();
        return destination.Write(indexSize)
            && destination.Write(vertexSize)
            && destination.WriteData(
                payload.GetIndexDataForAnalysis().data(), indexSize)
            && destination.WriteData(
                payload.GetVertexDataForAnalysis().data(), vertexSize);
    }

    bool spDXSharedMeshDataSerializer::ReadPayloadForAnalysis(
        spStream& source,
        spDXSharedMeshData& target) const
    {
        std::uint32_t indexSize = 0;
        std::uint32_t vertexSize = 0;
        if (!source.Read(indexSize) || !source.Read(vertexSize))
        {
            return false;
        }

        try
        {
            std::vector<std::byte> indices(indexSize);
            std::vector<std::byte> vertices(vertexSize);
            if (!source.ReadData(indices.data(), indexSize)
                || !source.ReadData(vertices.data(), vertexSize))
            {
                return false;
            }
            return target.InitializeForAnalysis(indices, vertices);
        }
        catch (...)
        {
            return false;
        }
    }
}
