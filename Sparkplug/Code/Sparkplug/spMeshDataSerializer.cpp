#include "spMeshDataSerializer.h"

#include "spMeshData.h"
#include "spDataBlockSerializer.h"
#include "spSerializerManager.h"
#include "../SparkBase/spMemoryStream.h"
#include "../SparkplugDX/spDXMesh.h"

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
            spRTTIManager::Instance().RegisterDeferredForAnalysis(MeshDataSerializerRecord);
    }

    spMeshDataSerializer::spMeshDataSerializer() noexcept
    {
        // Host static-library linkage: retain the wire target's registration
        // TU even though PC creates DXMesh instead. Native global CRT startup
        // is not being attributed to this reconstructed constructor.
        (void)spMeshData::StaticRTTI();
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

    std::unique_ptr<spBaseObject> spMeshDataSerializer::ReadObjectHeaderAndCreateForAnalysis(
        spStream& source, spSerializerObjectHeaderForAnalysis* observedHeader) const
    {
        // Actual PC42AFD0 ignores both header words and creates DXMesh, not the
        // MeshData RTTI factory. The common manager still validates FAT extents.
        spSerializerObjectHeaderForAnalysis header;
        if(!source.ReadData(&header,sizeof(header)))return nullptr;
        if(observedHeader)*observedHeader=header;
        try{return std::make_unique<spDXMesh>();}catch(...){return nullptr;}
    }

    bool spMeshDataSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t byteCount,spBaseObject& object,std::string* error) const
    {
        return ReadMeshFieldsForAnalysis(context,source,byteCount,object,false,error);
    }

    bool spMeshDataSerializer::ReadMeshFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t byteCount,spBaseObject& object,bool dxFields,std::string* error) const
    {
        if(error)error->clear();
        const auto fail=[&](const char* message){if(error)*error=message;return false;};
        auto* mesh=dynamic_cast<spDXMesh*>(&object);
        if(!mesh)return fail("PC mesh serializer requires the concrete DXMesh factory output");
        std::uint32_t start=0,size=0;
        if(!byteCount||byteCount>MaximumPayloadBytesForAnalysis||!source.GetCurrentPosition(start)||
            !source.GetSize(&size)||source.GetLogicalOriginForAnalysis()>size)
            return fail("Invalid bounded mesh field extent");
        size-=source.GetLogicalOriginForAnalysis();
        if(start>size||byteCount>size-start)return fail("Mesh fields exceed logical segment");
        const auto end=start+byteCount;
        const bool native=dxFields&&(context.manager.GetPlatformMaskForAnalysis()&spSerializerManager::PlatformPC)!=0;
        spDataBlockSerializer blocks;
        bool initialized=false;
        for(std::uint32_t fieldCount=0;fieldCount<65536;++fieldCount)
        {
            std::uint32_t position=0;
            if(!source.GetCurrentPosition(position)||position>=end)return fail("Missing mesh field terminator");
            const auto* header=blocks.ReadHeaderForAnalysis(source);
            if(!header||header->dataStreamPosition>end||header->payloadSize>end-header->dataStreamPosition)
                return fail("Truncated mesh field header/payload");
            if(header->IsTerminator())
            {
                // Native accepts missing fields/EOF too. Explicit host contract
                // refuses a blank mesh and requires complete declared framing.
                return initialized&&header->dataStreamPosition==end
                    ? true:fail("Mesh has no selected geometry field or has trailing bytes");
            }
            const auto selected=native?1u:0u;
            if(header->fieldID!=selected)
            {
                if(!spDataBlockSerializer::SkipDataForAnalysis(source,*header))return fail("Cannot skip mesh field");
                continue;
            }
            spMemoryStream payload;
            if(!payload.ResizeAndSetSize(header->payloadSize)||
                !source.ReadData(payload.GetBuffer(),header->payloadSize))return fail("Cannot read bounded mesh payload");
            if(native)
            {
                // Original429A40 discards this planning header. CPU buffer
                // headers, not these five numbers, determine actual decoding.
                std::uint32_t ignored=0;std::uint8_t ignoredByte=0;
                if(!payload.Read(ignored)||!payload.Read(ignored)||!payload.Read(ignored)||
                    !payload.Read(ignored)||!payload.Read(ignoredByte))return fail("Truncated native mesh planning header");
            }
            spIndexBuffer indices;spVertexBuffer vertices;
            if(!payload.GetCurrentPosition(position)||position>header->payloadSize||
                !indices.ReadForAnalysis(payload,header->payloadSize-position))return fail("Invalid bounded mesh index buffer");
            if(!payload.GetCurrentPosition(position)||position>header->payloadSize||
                !vertices.ReadForAnalysis(payload,header->payloadSize-position))return fail("Invalid bounded mesh vertex buffer");
            if(!payload.GetCurrentPosition(position)||position!=header->payloadSize)return fail("Unaccounted mesh payload bytes");
            if(!mesh->InitializeFromBuffersForAnalysis(indices,vertices,false,context.activeMeshCombiner,context.pcRenderer))
                return fail("Cannot initialize PC mesh buffers/ranges/declaration");
            initialized=true;
        }
        return fail("Mesh field-count bound exceeded");
    }

    bool spMeshDataSerializer::EmitsCrossPlatformPayloadForAnalysis(
        const std::uint32_t nativeSerializationMode) noexcept
    {
        return nativeSerializationMode == 0 || nativeSerializationMode == 2;
    }

    bool spMeshDataSerializer::IndexRelationshipsForAnalysis(spBaseObject& object) const
    {
        // PC5A7DB0 returns true; both CPU buffers are local owned payloads.
        return IsExactly(ClassID)&&dynamic_cast<spMeshData*>(&object)!=nullptr;
    }

    bool spMeshDataSerializer::WritePayloadForAnalysis(spStream& stream,
        const spBaseObject& object,std::string* error) const
    {
        spSerializerManager manager;
        return WritePayloadWithContextForAnalysis(manager,stream,object,error);
    }

    bool spMeshDataSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();
        const auto fail=[&](const char* message){if(error)*error=message;return false;};
        if(!IsExactly(ClassID))return fail("Derived mesh serializer requires its own verified writer");
        const auto* mesh=dynamic_cast<const spMeshData*>(&object);
        if(!mesh)return fail("MeshData writer requires CPU MeshData");
        spDataBlockSerializer fields;
        if(!fields.BeginObjectForAnalysis(stream,mesh))return fail("Cannot begin mesh fields");
        if(EmitsCrossPlatformPayloadForAnalysis(manager.GetSerializationPolicyForAnalysis()))
        {
            const auto* indices=mesh->GetIndexBufferForAnalysis();
            const auto* vertices=mesh->GetVertexBufferForAnalysis();
            if(!indices||!vertices||!indices->IsInitializedForAnalysis()||!vertices->IsInitializedForAnalysis()
                ||static_cast<std::uint64_t>(indices->GetIndexCountForAnalysis())*indices->GetIndexElementSizeForAnalysis()
                    +vertices->GetVertexSizeForAnalysis()>MaximumPayloadBytesForAnalysis)
                return fail("Invalid or oversized CPU mesh buffers");
            if(!fields.WriteBeginForAnalysis(0)||!indices->WriteForAnalysis(stream)
                ||!vertices->WriteForAnalysis(stream)||!fields.WriteEndForAnalysis(0))
                return fail("Cannot write cross-platform mesh field");
        }
        return fields.FinalizeObjectForAnalysis()?true:fail("Cannot finalize mesh fields");
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
