#include "spMeshBVSerializer.h"
#include "spMeshBV.h"
#include "Analysis/PC/spSectionCursor.h"
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> Create(){return std::make_unique<spMeshBVSerializer>();}
const spRTTIRecord Record{spMeshBVSerializer::ClassID,spSerializer::ClassID,"spMeshBVSerializer",
    &spSerializer::StaticRTTI(),&Create,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
}
const spRTTIRecord& spMeshBVSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& spMeshBVSerializer::vfunc_18() const noexcept{return Record;}
std::unique_ptr<spBaseObject> spMeshBVSerializer::vfunc_10(spCloneManager& manager) const {
    auto clone=std::make_unique<spMeshBVSerializer>();manager.RegisterCloneForAnalysis(*this,*clone);
    return vfunc_14(*clone,manager)?std::move(clone):nullptr;
}
std::unique_ptr<spCollisionMesh> spMeshBVSerializer::ReadGeometryForAnalysis(
    spStream& stream,std::uint32_t size,std::uint32_t* vertexPayloadOffset,std::string* error) {
    if(error)error->clear();
    const auto fail=[&](const char* message)->std::unique_ptr<spCollisionMesh>{if(error)*error=message;return nullptr;};
    std::uint32_t start=0,position=0;
    if(!stream.GetCurrentPosition(start))return fail("Cannot observe MeshBV geometry position");
    auto data=CreateGeometryForAnalysis(std::make_unique<spIndexBuffer>(),std::make_unique<spVertexBuffer>());
    if(!data->indices_->ReadForAnalysis(stream,size))return fail("Invalid MeshBV index buffer");
    if(!stream.GetCurrentPosition(position)||position<start||position-start>size)
        return fail("MeshBV index extent overflow");
    if(!data->vertices_->ReadForAnalysis(stream,size-(position-start)))return fail("Invalid MeshBV vertex buffer");
    if(vertexPayloadOffset) {
        if(!stream.GetCurrentPosition(position)||position<start+data->vertices_->GetVertexSizeForAnalysis())
            return fail("Cannot observe MeshBV vertex position");
        *vertexPayloadOffset=position-start-data->vertices_->GetVertexSizeForAnalysis();
    }
    return data;
}
std::unique_ptr<spCollisionMesh> spMeshBVSerializer::CreateGeometryForAnalysis(
    std::unique_ptr<spIndexBuffer> indices,std::unique_ptr<spVertexBuffer> vertices) {
    auto data=std::make_unique<spCollisionMesh>();
    data->indices_=std::move(indices);data->vertices_=std::move(vertices);
    return data;
}
bool spMeshBVSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
    spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const {
    if(error)error->clear();
    evidence::pc::serialization::SectionCursor cursor(context,stream,size,true,error);
    auto* mesh=dynamic_cast<spMeshBV*>(&object);if(!mesh)return cursor.Fail("MeshBV target mismatch");
    mesh->serializedFieldMask_=0;mesh->vertexPayloadOffset_=0;
    std::uint32_t payloadStart=0;if(!stream.GetCurrentPosition(payloadStart))return cursor.Fail("Cannot observe MeshBV payload");
    spCollisionMesh* current=nullptr; // native local set by field0, not old object data
    while(const auto* header=cursor.Next()) {
        if(header->IsTerminator())return true;
        if(header->fieldID==0) {
            std::uint32_t vertexOffset=0;
            auto data=ReadGeometryForAnalysis(stream,header->payloadSize,&vertexOffset,error);
            if(!data){context.failed=true;return false;}
            mesh->serializedFieldMask_|=1;
            mesh->vertexPayloadOffset_=header->dataStreamPosition-payloadStart+vertexOffset;
            current=data.get();
            if(!mesh->SetDataAndBoundsForAnalysis(std::move(data)))return cursor.Fail("Unsupported or invalid MeshBV geometry");
        } else if(header->fieldID==1) {
            if(!current)return cursor.Fail("MeshBV face data precedes geometry"); // native would dereference null
            auto faces=std::make_unique<spFaceDataContainer>();
            if(!faces->ReadForAnalysis(stream,header->payloadSize))return cursor.Fail("Invalid or unregistered MeshBV face data");
            current->faces_=std::move(faces);
            mesh->serializedFieldMask_|=2;
        } else if(!cursor.Skip())return cursor.Fail("Cannot skip MeshBV field");
    }
    return false;
}
bool spMeshBVSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const {
    if(error)error->clear();const auto fail=[&](const char* message){if(error)*error=message;return false;};
    const auto* mesh=dynamic_cast<const spMeshBV*>(&object);const auto* data=mesh?mesh->GetDataForAnalysis():nullptr;
    if(!data||!data->GetIndicesForAnalysis()||!data->GetVerticesForAnalysis())return fail("MeshBV writer requires geometry");
    spDataBlockSerializer blocks;
    if(!blocks.BeginObjectForAnalysis(stream,&object)||!blocks.WriteBeginForAnalysis(0)
        ||!data->GetIndicesForAnalysis()->WriteForAnalysis(stream)||!data->GetVerticesForAnalysis()->WriteForAnalysis(stream)
        ||!blocks.WriteEndForAnalysis(0))return fail("Cannot write MeshBV geometry");
    if(const auto* faces=data->GetFacesForAnalysis()) {
        if(!blocks.WriteBeginForAnalysis(1)||!faces->WriteForAnalysis(stream)||!blocks.WriteEndForAnalysis(1))return fail("Cannot write MeshBV face data");
    }
    return blocks.FinalizeObjectForAnalysis();
}
}
