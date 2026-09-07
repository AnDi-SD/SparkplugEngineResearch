#include "spDXMeshDataSerializer.h"

#include "spDXMeshData.h"
#include "spMeshData.h"
#include "spDataBlockSerializer.h"
#include "spSerializerManager.h"
#include "../SparkBase/spStream.h"

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
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXMeshDataSerializerRecord);
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

    bool spDXMeshDataSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t byteCount,spBaseObject& object,std::string* error) const
    {
        return ReadMeshFieldsForAnalysis(context,source,byteCount,object,true,error);
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

    bool spDXMeshDataSerializer::IndexRelationshipsForAnalysis(spBaseObject& object) const
    {
        return dynamic_cast<spMeshData*>(&object)!=nullptr;
    }

    bool spDXMeshDataSerializer::WritePayloadForAnalysis(spStream& stream,
        const spBaseObject& object, std::string* error) const
    {
        spSerializerManager manager;
        return WritePayloadWithContextForAnalysis(manager, stream, object, error);
    }

    bool spDXMeshDataSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& stream, const spBaseObject& object, std::string* error) const
    {
        if(error)error->clear();
        const auto fail=[&](const char* message){if(error)*error=message;return false;};
        const auto* mesh=dynamic_cast<const spMeshData*>(&object);
        if(!mesh)return fail("DX mesh writer requires CPU MeshData");
        const auto header=BuildNativePayloadHeaderForAnalysis(*mesh);
        if(!header.valid || static_cast<std::uint64_t>(header.vertexDataSize)+header.indexDataSize
            >MaximumPayloadBytesForAnalysis)return fail("Invalid or oversized CPU mesh buffers");
        spDataBlockSerializer fields;
        if(!fields.BeginObjectForAnalysis(stream,mesh))return fail("Cannot begin mesh fields");
        if(EmitsCrossPlatformPayloadForAnalysis(manager.GetSerializationPolicyForAnalysis()))
        {
            if(!fields.WriteBeginForAnalysis(0)
                ||!mesh->GetIndexBufferForAnalysis()->WriteForAnalysis(stream)
                ||!mesh->GetVertexBufferForAnalysis()->WriteForAnalysis(stream)
                ||!fields.WriteEndForAnalysis(0))return fail("Cannot write cross-platform mesh field");
        }
        // PC4298A0 -> 013BC5E0 constructs a temporary DXMeshData copy.
        // Its CPU buffers remain packed; only this 17-byte planning header
        // anticipates the 12 extra bytes per vertex for component bit 0x20.
        spDXMeshData native;
        if(!fields.WriteBeginForAnalysis(1)||!native.InitializeFromMeshDataForAnalysis(*mesh))
            return fail("Cannot prepare native mesh field");
        const std::uint32_t words[]{header.fvfCode,header.vertexCount,header.vertexDataSize,header.indexDataSize};
        const auto wide=static_cast<std::uint8_t>(header.indicesAre32Bit);
        if(!stream.WriteData(words,sizeof(words))||!stream.Write(wide)
            ||!native.GetIndexBufferForAnalysis()->WriteForAnalysis(stream)
            ||!native.GetVertexBufferForAnalysis()->WriteForAnalysis(stream)
            ||!fields.WriteEndForAnalysis(1)||!fields.FinalizeObjectForAnalysis())
            return fail("Cannot write native mesh field");
        return true;
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
