#include "spPartitionRenderableSerializer.h"
#include "spPartitionRenderable.h"
#include "Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction
{
    using namespace evidence::pc::serialization;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPartitionRenderableSerializer>();}
        const spRTTIRecord Record{spPartitionRenderableSerializer::ClassID,spSerializer::ClassID,"spPartitionRenderableSerializer",&spSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spPartitionRenderableSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spPartitionRenderableSerializer::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spPartitionRenderableSerializer::vfunc_10(spCloneManager&) const
    {
        // Serializer cloning is outside this read slice; no guessed clone body.
        return nullptr;
    }

bool spPartitionRenderableSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
    spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
{
    auto* partition=dynamic_cast<spPartitionRenderable*>(&object);
    if(!partition||!object.IsKindOf(spPartitionRenderable::ClassID))
    {context.failed=true;if(error)*error="PartitionRenderable target mismatch";return false;}
    SectionCursor cursor(context,source,size,true,error);
    while(const auto* header=cursor.Next())
    {
        if(header->IsTerminator())return true;
        if(header->fieldID==1)
        {
            std::uint32_t color=0;
            if(!cursor.Read(color))return cursor.Fail("Invalid PartitionRenderable debug color");
            partition->SetDebugColorForAnalysis(color);
        }
        else if(header->fieldID==0)
        {
            auto* value=ReadFieldReferenceForAnalysis(context,spRenderable::ClassID,source,*header,error);
            if(context.failed)return false;
            auto owner=std::dynamic_pointer_cast<spRenderable>(context.ShareObjectForAnalysis(value));
            if(!owner)return cursor.Fail("Null renderable or missing explicit owner");
            auto* renderable=owner.get();
            if(!partition->AttachRenderableForAnalysis(std::move(owner)))return cursor.Fail("Cannot attach partition renderable");
            renderable->SetField28ForAnalysis(partition->GetDebugColorForAnalysis()); //44EDBF, AFTER append
        }
        else if(!cursor.Skip())return cursor.Fail("Cannot skip PartitionRenderable field");
    }
    return false;
}
}
