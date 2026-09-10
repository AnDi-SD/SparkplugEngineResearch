#include "spZonePortalNodeSerializer.h"
#include "spSerializerClone.h"
#include "spZonePortalNode.h"
#include "Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction
{
    using namespace evidence::pc::serialization;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spZonePortalNodeSerializer>();}
        const spRTTIRecord Record{spZonePortalNodeSerializer::ClassID,spNodeSerializer::ClassID,"spZonePortalNodeSerializer",&spNodeSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spZonePortalNodeSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spZonePortalNodeSerializer::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spZonePortalNodeSerializer::vfunc_10(spCloneManager& manager) const
    {
        return CloneConcreteSerializerForAnalysis(*this, Create(), manager);
    }
    spClassID spZonePortalNodeSerializer::GetTargetClassIDForAnalysis() const noexcept{return spZonePortalNode::ClassID;}

bool spZonePortalNodeSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
    spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
{
    auto* node=dynamic_cast<spZonePortalNode*>(&object);std::uint32_t start=0,remaining=0;
    if(!node||!object.IsExactly(spZonePortalNode::ClassID)||!source.GetCurrentPosition(start)
        ||!ReadNodeFieldsForAnalysis(context,source,size,*node,false,error)
        ||!RemainingSection(source,start,size,remaining))
    {context.failed=true;if(error&&error->empty())*error="Missing ZonePortalNode derived section";return false;}
    SectionCursor cursor(context,source,remaining,true,error);
    while(const auto* header=cursor.Next())
    {
        if(header->IsTerminator())return true;
        if(header->fieldID!=0){if(!cursor.Skip())return cursor.Fail("Cannot skip ZonePortalNode field");continue;}
        auto* value=ReadFieldReferenceForAnalysis(context,spZonePortal::ClassID,source,*header,error);
        if(context.failed)return false;
        auto* portal=dynamic_cast<spZonePortal*>(value);
        if(!portal||!node->AppendPortalForAnalysis(portal))return cursor.Fail("Invalid borrowed ZonePortalNode portal");
    }
    return false;
}
}
