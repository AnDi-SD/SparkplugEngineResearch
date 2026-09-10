#include "spPartitionSystemSerializer.h"
#include "spSerializerClone.h"
#include "spPartitionSystem.h"
#include "Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction
{
    using namespace evidence::pc::serialization;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPartitionSystemSerializer>();}
        const spRTTIRecord Record{spPartitionSystemSerializer::ClassID,spRenderNodeSerializer::ClassID,"spPartitionSystemSerializer",&spRenderNodeSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spPartitionSystemSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spPartitionSystemSerializer::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spPartitionSystemSerializer::vfunc_10(spCloneManager& manager) const
    {
        return CloneConcreteSerializerForAnalysis(*this, Create(), manager);
    }
    spClassID spPartitionSystemSerializer::GetTargetClassIDForAnalysis() const noexcept{return spPartitionSystem::ClassID;}

bool spPartitionSystemSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
    spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
{
    auto* system=dynamic_cast<spPartitionSystem*>(&object);std::uint32_t start=0,remaining=0;
    if(!system||!object.IsExactly(spPartitionSystem::ClassID)||!source.GetCurrentPosition(start)
        ||!ReadRenderNodeFieldsForAnalysis(context,source,size,*system,false,error)
        ||!RemainingSection(source,start,size,remaining))
    {context.failed=true;if(error&&error->empty())*error="Missing PartitionSystem derived section";return false;}
    SectionCursor cursor(context,source,remaining,true,error);
    while(const auto* header=cursor.Next())
    {
        if(header->IsTerminator())return true;
        if(header->fieldID!=0){if(!cursor.Skip())return cursor.Fail("Cannot skip PartitionSystem field");continue;}
        auto* value=ReadFieldReferenceForAnalysis(context,spPartitionNode::ClassID,source,*header,error);
        if(context.failed)return false;
        if(!value)return cursor.Fail("Null partition root");
        if(system->GetPartitionRootForAnalysis()==value)continue;
        if(system->GetPartitionRootForAnalysis())return cursor.Fail("Replacing a distinct direct root is outside safe host ownership");
        auto owner=TakeSpatialOwner<spPartitionNode>(context,value,*system,error);
        if(!owner)return false;
        if(!system->SetPartitionRootForAnalysis(std::move(owner)))return cursor.Fail("Cannot assign partition root");
    }
    return false;
}
}
