#include "spBSPNodeSerializer.h"
#include "spSerializerClone.h"
#include "spBSPNode.h"
#include "Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction
{
    using namespace evidence::pc::serialization;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spBSPNodeSerializer>();}
        const spRTTIRecord Record{spBSPNodeSerializer::ClassID,spPartitionNodeSerializer::ClassID,"spBSPNodeSerializer",&spPartitionNodeSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spBSPNodeSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spBSPNodeSerializer::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spBSPNodeSerializer::vfunc_10(spCloneManager& manager) const
    {
        return CloneConcreteSerializerForAnalysis(*this, Create(), manager);
    }

bool spBSPNodeSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
    spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
{
    auto* node=dynamic_cast<spBSPNode*>(&object);std::uint32_t start=0,remaining=0;
    if(!node||!object.IsExactly(spBSPNode::ClassID)||!source.GetCurrentPosition(start)
        ||!ReadPartitionFieldsForAnalysis(context,source,size,*node,false,error)
        ||!RemainingSection(source,start,size,remaining))
    {context.failed=true;if(error&&error->empty())*error="Missing BSP derived section";return false;}
    SectionCursor cursor(context,source,remaining,true,error);
    while(const auto* header=cursor.Next())
    {
        if(header->IsTerminator())return true;
        if(header->fieldID==0)
        {
            spBSPNode::Plane plane;
            if(!cursor.Read(plane))return cursor.Fail("Invalid BSP plane");
            node->SetPlaneForAnalysis(plane); // raw16, independent of polygon
        }
        else if(header->fieldID==1)
        {
            std::vector<spBSPNode::Vector3> points;
            if(!ReadSpatialPolygon(source,*header,points)||!node->SetPolygonForAnalysis(points))
                return cursor.Fail("Invalid BSP polygon");
        }
        else if(!cursor.Skip())return cursor.Fail("Cannot skip BSP field");
    }
    return false;
}
}
