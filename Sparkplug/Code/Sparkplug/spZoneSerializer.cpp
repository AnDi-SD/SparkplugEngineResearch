#include "spZoneSerializer.h"
#include "spZone.h"
#include "spPartitionNode.h"
#include "Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction
{
    using namespace evidence::pc::serialization;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spZoneSerializer>();}
        const spRTTIRecord Record{spZoneSerializer::ClassID,spNodeSerializer::ClassID,"spZoneSerializer",&spNodeSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spZoneSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spZoneSerializer::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spZoneSerializer::vfunc_10(spCloneManager&) const
    {
        // Serializer cloning is outside this read slice; no guessed clone body.
        return nullptr;
    }
    spClassID spZoneSerializer::GetTargetClassIDForAnalysis() const noexcept{return spZone::ClassID;}

bool spZoneSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
    spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
{
    auto* zone=dynamic_cast<spZone*>(&object);std::uint32_t start=0,remaining=0;
    if(!zone||!object.IsExactly(spZone::ClassID)||!source.GetCurrentPosition(start)
        ||!ReadNodeFieldsForAnalysis(context,source,size,*zone,false,error)
        ||!RemainingSection(source,start,size,remaining))
    {context.failed=true;if(error&&error->empty())*error="Missing Zone derived section";return false;}
    SectionCursor cursor(context,source,remaining,true,error);
    while(const auto* header=cursor.Next())
    {
        if(header->IsTerminator())return true;
        if(header->fieldID!=0){if(!cursor.Skip())return cursor.Fail("Cannot skip Zone field");continue;}
        auto* value=ReadFieldReferenceForAnalysis(context,spPartitionNode::ClassID,source,*header,error);
        if(context.failed)return false;
        auto* root=dynamic_cast<spPartitionNode*>(value);
        if(!root||!zone->AppendRootForAnalysis(root))return cursor.Fail("Invalid borrowed Zone root");
    }
    return false;
}
}
