#include "spOcclusionVolumeSerializer.h"
#include "../../Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> Create(){return std::make_unique<spOcclusionVolumeSerializer>();}
const spRTTIRecord Record{spOcclusionVolumeSerializer::ClassID,spNodeSerializer::ClassID,"spOcclusionVolumeSerializer",&spNodeSerializer::StaticRTTI(),&Create,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
}
const spRTTIRecord& spOcclusionVolumeSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& spOcclusionVolumeSerializer::vfunc_18() const noexcept{return Record;}
bool spOcclusionVolumeSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,spStream& source,
    std::uint32_t size,spBaseObject& object,std::string* error) const
{
    using namespace evidence::pc::serialization;
    auto* volume=dynamic_cast<spOcclusionVolume*>(&object);std::uint32_t start=0,remaining=0;
    if(!volume||!source.GetCurrentPosition(start)||!ReadNodeFieldsForAnalysis(context,source,size,*volume,false,error)||!RemainingSection(source,start,size,remaining)) {
        context.failed=true;if(error&&error->empty())*error="Missing Occlusion inherited/own section";return false;
    }
    SectionCursor cursor(context,source,remaining,true,error);
    std::unique_ptr<spIndexBuffer> indices;std::unique_ptr<spVertexBuffer> vertices;
    while(const auto* field=cursor.Next()) {
        if(field->IsTerminator()) {
            if(!indices||!vertices)return cursor.Fail("Occlusion requires both CPU buffers");
            if(!volume->InitializeForAnalysis(*indices,*vertices,sort_,error)){context.failed=true;return false;}
            return true; // original44F595 deletes both temporary input buffers
        }
        if(field->fieldID==0) {
            if(indices)return cursor.Fail("Repeated Occlusion IB has unsupported original lifetime");
            indices=std::make_unique<spIndexBuffer>();
            if(!indices->ReadForAnalysis(source,field->payloadSize))return cursor.Fail("Invalid Occlusion IB");
        } else if(field->fieldID==1) {
            if(vertices)return cursor.Fail("Repeated Occlusion VB has unsupported original lifetime");
            vertices=std::make_unique<spVertexBuffer>();
            if(!vertices->ReadForAnalysis(source,field->payloadSize))return cursor.Fail("Invalid Occlusion VB");
        } else if(!cursor.Skip())return cursor.Fail("Cannot skip Occlusion field");
    }
    return false;
}
bool spOcclusionVolumeSerializer::WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string* error) const
{if(error)*error="Occlusion writer is not reconstructed";return false;}
bool spOcclusionVolumeSerializer::WritePayloadWithContextForAnalysis(spSerializerManager&,spStream& output,const spBaseObject& object,std::string* error) const
{return WritePayloadForAnalysis(output,object,error);}
}
