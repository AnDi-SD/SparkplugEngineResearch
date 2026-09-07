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

        bool ReadBoundedSizes(spStream& source, std::uint32_t& indexSize,
            std::uint32_t& vertexSize, std::uint32_t& position)
        {
            if (!source.Read(indexSize) || !source.Read(vertexSize)
                || !source.GetCurrentPosition(position))
            {
                return false;
            }
            std::uint32_t fileSize = 0;
            const auto payloadSize = static_cast<std::uint64_t>(indexSize) + vertexSize;
            const auto physicalPosition = static_cast<std::uint64_t>(position)
                + source.GetLogicalOriginForAnalysis();
            // Validate both arithmetic and input extent before either vector
            // allocation. The native raw-pointer reader has no such guard.
            return payloadSize <= spDXSharedMeshDataSerializer::MaximumPayloadBytesForAnalysis
                && source.GetSize(&fileSize) && physicalPosition <= fileSize
                && payloadSize <= fileSize - physicalPosition;
        }
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
        std::uint32_t position = 0;
        if (!ReadBoundedSizes(source, indexSize, vertexSize, position))
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

    bool spDXSharedMeshDataSerializer::ReadContiguousPayloadForAnalysis(
        spStream& source, spDXSharedMeshData& target) const
    {
        if (source.GetLogicalOriginForAnalysis() != 0)
        {
            return false;
        }
        const auto* buffer = static_cast<const std::byte*>(source.GetBuffer());
        if (buffer == nullptr)
        {
            return false;
        }
        std::uint32_t indexSize = 0, vertexSize = 0, position = 0;
        if (!ReadBoundedSizes(source, indexSize, vertexSize, position))
        {
            return false;
        }
        try
        {
            const auto* indices = buffer + position;
            const auto* vertices = indices + indexSize;
            return target.InitializeForAnalysis(
                std::vector<std::byte>(indices, vertices),
                std::vector<std::byte>(vertices, vertices + vertexSize));
        }
        catch (...)
        {
            return false;
        }
    }
}
