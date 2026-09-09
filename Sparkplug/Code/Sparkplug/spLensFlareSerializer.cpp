#include "spLensFlareSerializer.h"
#include "spLensFlare.h"
#include "spMaterial.h"
#include "spRenderNode.h"
#include "Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> Create(){return std::make_unique<spLensFlareSerializer>();}
const spRTTIRecord Record{spLensFlareSerializer::ClassID,spRenderableSerializer::ClassID,"spLensFlareSerializer",&spRenderableSerializer::StaticRTTI(),&Create,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
}
const spRTTIRecord& spLensFlareSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& spLensFlareSerializer::vfunc_18() const noexcept{return Record;}
bool spLensFlareSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const {
    using namespace evidence::pc::serialization;
    auto* flare=dynamic_cast<spLensFlare*>(&object);std::uint32_t start=0,remaining=0;
    if(!flare||!source.GetCurrentPosition(start)||!ReadRenderableFieldsForAnalysis(context,source,size,*flare,false,error)||!RemainingSection(source,start,size,remaining)){
        context.failed=true;if(error&&error->empty())*error="Missing LensFlare inherited/own section";return false;}
    SectionCursor cursor(context,source,remaining,true,error);
    const auto element=[&](std::uint32_t end,spLensFlare::ElementForAnalysis& value,bool preserveNull)->bool {
        auto* raw=ReadSequenceReferenceForAnalysis(context,spMaterial::ClassID,source,end,error);
        if(context.failed)return false;
        auto material=std::dynamic_pointer_cast<spMaterial>(context.ShareObjectForAnalysis(raw));
        if(raw&&!material)return false;
        std::uint32_t position=0,color=0;float distance=0,scale=0;
        if(!source.GetCurrentPosition(position)||position>end||end-position<12||!source.Read(color)||!source.Read(distance)||!source.Read(scale))return false;
        // Field0 calls SetPrimary only for nonnull Material. Field1 always calls
        // indexed SetElement, including a null material and its raw tail.
        if(raw||!preserveNull){value.quad.SetMaterialForAnalysis(std::move(material));value.color=color;value.distance=distance;value.scale=scale;}
        return true;
    };
    while(const auto* field=cursor.Next()){
        if(field->IsTerminator())return true;
        const auto end=field->dataStreamPosition+field->payloadSize;
        if(field->fieldID==0){if(!element(end,flare->primary_,true))return cursor.Fail("Invalid bounded primary LensFlare element");}
        else if(field->fieldID==1){
            std::uint32_t count=0;if(field->payloadSize<4||!source.Read(count)||count>4096)return cursor.Fail("LensFlare element count exceeds host bound");
            // Original vector resize preserves existing elements and constructs
            // only the appended suffix; each serialized slot is then overwritten.
            flare->elements_.resize(count);for(auto& value:flare->elements_){if(!value)value=std::make_unique<spLensFlare::ElementForAnalysis>();if(!element(end,*value,false))return cursor.Fail("Invalid bounded counted LensFlare element");}
        }
        else if(field->fieldID==2){if(field->payloadSize!=8||!source.Read(flare->radius_)||!source.Read(flare->speed_))return cursor.Fail("Invalid LensFlare occlusion fields");}
        else if(field->fieldID==3){auto* raw=ReadFieldReferenceForAnalysis(context,spRenderNode::ClassID,source,*field,error);if(context.failed)return false;auto* node=dynamic_cast<spRenderNode*>(raw);if(raw&&!node)return cursor.Fail("LensFlare render-node relationship has unsupported type");flare->renderNode_=node;}
        else if(!cursor.Skip())return cursor.Fail("Cannot skip LensFlare field");
    }
    return false;
}
bool spLensFlareSerializer::WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string* error) const {if(error)*error="LensFlare writer is not reconstructed";return false;}
bool spLensFlareSerializer::WritePayloadWithContextForAnalysis(spSerializerManager&,spStream& stream,const spBaseObject& object,std::string* error) const {return WritePayloadForAnalysis(stream,object,error);}
}
