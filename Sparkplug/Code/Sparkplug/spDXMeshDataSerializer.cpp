#include "spDXMeshDataSerializer.h"

#include "spDXMeshData.h"
#include "spMeshData.h"

#include <limits>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXMeshDataSerializer()
        {
            return std::make_unique<spDXMeshDataSerializer>();
        }

        const spRTTIRecord DXMeshDataSerializerRecord{
            spDXMeshDataSerializer::ClassID,
            spMeshDataSerializer::ClassID,
            "spDXMeshDataSerializer",
            &spMeshDataSerializer::StaticRTTI(),
            &CreateDXMeshDataSerializer,
            nullptr,
        };

        const bool DXMeshDataSerializerRegistered =
            spRTTIManager::Instance().Register(DXMeshDataSerializerRecord);
    }

    spDXMeshDataSerializer::~spDXMeshDataSerializer() = default;

    const spRTTIRecord& spDXMeshDataSerializer::StaticRTTI() noexcept
    {
        (void)DXMeshDataSerializerRegistered;
        return DXMeshDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spDXMeshDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXMeshDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXMeshDataSerializer::vfunc_18() const noexcept
    {
        return DXMeshDataSerializerRecord;
    }

    spClassID spDXMeshDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spDXMeshData::ClassID;
    }

    std::vector<spDXMeshDataSerializer::Field>
    spDXMeshDataSerializer::BuildKnownWritePlanForAnalysis(
        const std::uint32_t nativeSerializationMode) const
    {
        std::vector<Field> plan;
        if (EmitsCrossPlatformPayloadForAnalysis(nativeSerializationMode))
        {
            plan.push_back(Field::CrossPlatform);
        }
        plan.push_back(Field::PlatformSpecific);
        return plan;
    }

    bool spDXMeshDataSerializer::PCLoadsNativePayloadForAnalysis(
        const std::uint32_t readerFlags) noexcept
    {
        return (readerFlags & PCNativeLoadFlagMask) != 0;
    }

    bool spDXMeshDataSerializer::PS2LoadsNativePayloadForAnalysis(
        const std::uint32_t readerFlags) noexcept
    {
        return (readerFlags & PS2NativeLoadFlagMask) != 0;
    }

    spDXMeshDataSerializer::NativePayloadHeader
    spDXMeshDataSerializer::BuildNativePayloadHeaderForAnalysis(
        const spMeshData& meshData) noexcept
    {
        NativePayloadHeader result;
        const auto* const vertices = meshData.GetVertexBufferForAnalysis();
        const auto* const indices = meshData.GetIndexBufferForAnalysis();
        if (vertices == nullptr || indices == nullptr
            || !vertices->IsInitializedForAnalysis()
            || !indices->IsInitializedForAnalysis())
        {
            return result;
        }

        result.fvfCode = vertices->GetComponentFlagsForAnalysis();
        result.vertexCount = vertices->GetVertexCountForAnalysis();
        result.indicesAre32Bit =
            indices->GetIndexElementSizeForAnalysis() == sizeof(std::uint32_t);

        std::uint64_t vertexBytes = vertices->GetVertexSizeForAnalysis();
        if ((result.fvfCode & 0x20U) != 0)
        {
            vertexBytes += static_cast<std::uint64_t>(result.vertexCount)
                * ExpandedField30BytesPerVertex;
        }
        const std::uint64_t indexBytes =
            static_cast<std::uint64_t>(indices->GetIndexCountForAnalysis())
            * indices->GetIndexElementSizeForAnalysis();
        if (vertexBytes > std::numeric_limits<std::uint32_t>::max()
            || indexBytes > std::numeric_limits<std::uint32_t>::max())
        {
            return result;
        }

        result.vertexDataSize = static_cast<std::uint32_t>(vertexBytes);
        result.indexDataSize = static_cast<std::uint32_t>(indexBytes);
        result.valid = true;
        return result;
    }
}
