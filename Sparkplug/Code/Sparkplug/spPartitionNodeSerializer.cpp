#include "spPartitionNodeSerializer.h"
#include "spSerializerClone.h"
#include "spPartitionNode.h"
#include "Analysis/PC/spSpatialReadSupport.h"
#include "spPartitionSystem.h"
#include "spPartitionRenderable.h"
#include "spStaticRenderObject.h"
#include "spZone.h"
#include "spZonePortal.h"
#include "spCollisionInfo.h"
namespace sparkplug::reconstruction
{
    using namespace evidence::pc::serialization;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPartitionNodeSerializer>();}
        const spRTTIRecord Record{spPartitionNodeSerializer::ClassID,spSerializer::ClassID,"spPartitionNodeSerializer",&spSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spPartitionNodeSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spPartitionNodeSerializer::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spPartitionNodeSerializer::vfunc_10(spCloneManager& manager) const
    {
        return CloneConcreteSerializerForAnalysis(*this, Create(), manager);
    }

bool spPartitionNodeSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
    spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
{
    auto* node=dynamic_cast<spPartitionNode*>(&object);
    if(!node||!object.IsExactly(spPartitionNode::ClassID))
    {context.failed=true;if(error)*error="PartitionNode target mismatch";return false;}
    return ReadPartitionFieldsForAnalysis(context,source,size,*node,true,error);
}
bool spPartitionNodeSerializer::ReadPartitionFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
    spStream& source,std::uint32_t size,spPartitionNode& node,bool exact,std::string* error) const
{
    SectionCursor cursor(context,source,size,exact,error);
    while(const auto* header=cursor.Next())
    {
        if(header->IsTerminator())return true;
        if(header->fieldID==1)
        {
            std::uint32_t value=0;if(!cursor.Read(value))return cursor.Fail("Invalid partition debug color");
            node.SetDebugColorForAnalysis(value);continue;
        }
        if(header->fieldID==2)
        {
            std::uint32_t slot=0;
            if(header->payloadSize<8||!source.Read(slot)||slot>=node.GetChildCountForAnalysis())
                return cursor.Fail("Partition child slot exceeds actual factory storage");
            auto* value=ReadSequenceReferenceForAnalysis(context,spPartitionNode::ClassID,source,
                header->dataStreamPosition+header->payloadSize,error);
            if(context.failed)return false;
            if(!value)return cursor.Fail("Null partition child");
            if(node.GetChildForAnalysis(slot)==value)continue; // repeated identical assignment
            if(node.GetChildForAnalysis(slot))return cursor.Fail("Replacing a distinct direct child is outside safe host ownership");
            auto owner=TakeSpatialOwner<spPartitionNode>(context,value,node,error);
            if(!owner)return false;
            if(!node.SetChildForAnalysis(slot,std::move(owner)))return cursor.Fail("Cannot assign partition child");
            continue;
        }
        spClassID expected=0;
        switch(header->fieldID)
        {
        case 0:expected=spCollisionInfo::ClassID;break;
        case 3:expected=spZone::ClassID;break;
        case 4:expected=spZonePortal::ClassID;break;
        case 5:expected=spPartitionSystem::ClassID;break;
        case 6:expected=spPartitionRenderable::ClassID;break;
        case 7:expected=spStaticRenderObject::ClassID;break;
        default:if(!cursor.Skip())return cursor.Fail("Cannot skip partition field");continue;
        }
        auto* value=ReadFieldReferenceForAnalysis(context,expected,source,*header,error);
        if(context.failed)return false;
        switch(header->fieldID)
        {
        case 0:
        {
            auto* collision=dynamic_cast<spCollisionInfo*>(value);
            if(!collision||!collision->UpdateWorldForAnalysis()||!node.InsertCollisionForAnalysis(collision))
                return cursor.Fail("Partition collision is invalid or derived placement is not reconstructed");
            break;
        }
        case 3:
        {
            auto owner=std::dynamic_pointer_cast<spZone>(context.ShareObjectForAnalysis(value));
            if(value&&!owner)return cursor.Fail("Zone reference lacks the actual type or explicit owner");
            node.SetZoneForAnalysis(std::move(owner));break; // NULL clears actual intrusive Zone
        }
        case 4:
        {
            auto owner=std::dynamic_pointer_cast<spZonePortal>(context.ShareObjectForAnalysis(value));
            if(!owner||!node.AppendPortalForAnalysis(std::move(owner)))return cursor.Fail("Invalid partition portal");
            break;
        }
        case 5:
        {
            auto* system=dynamic_cast<spPartitionSystem*>(value);
            if(!system)return cursor.Fail("Null or wrong partition system");
            node.SetPartitionSystemForAnalysis(system);break;
        }
        case 6:
        {
            if(!value)break; // original NULL leaves old payload untouched
            if(node.GetPartitionRenderableForAnalysis()==value)break;
            if(node.GetPartitionRenderableForAnalysis())return cursor.Fail("Replacing a distinct direct payload is outside safe host ownership");
            auto owner=TakeSpatialOwner<spPartitionRenderable>(context,value,node,error);
            if(!owner)return false;
            if(!node.SetPartitionRenderableForAnalysis(std::move(owner)))return cursor.Fail("Cannot assign partition payload");
            break;
        }
        case 7:
        {
            if(!value)break; // actual NULL ignored
            auto owner=std::dynamic_pointer_cast<spStaticRenderObject>(context.ShareObjectForAnalysis(value));
            if(!owner||!node.InsertStaticForAnalysis(std::move(owner)))
                return cursor.Fail("Invalid static object or derived placement is not reconstructed");
            break;
        }
        }
    }
    return false;
}
}
