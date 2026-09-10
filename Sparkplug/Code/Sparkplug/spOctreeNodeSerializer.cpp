#include "spOctreeNodeSerializer.h"
#include "spSerializerClone.h"
#include "spOctreeNode.h"
#include "Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction
{
    using namespace evidence::pc::serialization;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spOctreeNodeSerializer>();}
        const spRTTIRecord Record{spOctreeNodeSerializer::ClassID,spPartitionNodeSerializer::ClassID,
            "spOctreeNodeSerializer",&spPartitionNodeSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spOctreeNodeSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spOctreeNodeSerializer::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spOctreeNodeSerializer::vfunc_10(spCloneManager& manager) const
    {
        return CloneConcreteSerializerForAnalysis(*this, Create(), manager);
    }

    bool spOctreeNodeSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        auto* node=dynamic_cast<spOctreeNode*>(&object);std::uint32_t start=0,remaining=0;
        if(!node||!object.IsExactly(spOctreeNode::ClassID)||!source.GetCurrentPosition(start)
            ||!ReadPartitionFieldsForAnalysis(context,source,size,*node,false,error)
            ||!RemainingSection(source,start,size,remaining))
        {context.failed=true;if(error&&error->empty())*error="Missing Octree derived section";return false;}
        return ReadOctreeFieldsForAnalysis(context,source,remaining,*node,error);
    }
    bool spOctreeNodeSerializer::ReadOctreeFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spOctreeNode& node,std::string* error) const
    {
        SectionCursor cursor(context,source,size,true,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID<=2)
            {
                spOctreeNode::Vector3 value;
                if(!cursor.Read(value))return cursor.Fail("Invalid Octree vector extent");
                // Original44C8E0 copies raw vectors independently. No midpoint,
                // finite/order validation or omitted-field reset is performed.
                if(header->fieldID==0){node.pivot_=value;node.geometryKnown_=true;}
                else if(header->fieldID==1)node.mins_=value;
                else node.maxs_=value;
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip Octree field");
        }
        return false;
    }
}
