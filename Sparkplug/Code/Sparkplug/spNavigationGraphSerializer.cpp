#include "spNavigationGraphSerializer.h"
#include "spNavigationGraph.h"
#include "spNavigationSet.h"
#include "spNavigationPortal.h"
#include "Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction
{
    using namespace evidence::pc::serialization;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spNavigationGraphSerializer>();}
        const spRTTIRecord Record{spNavigationGraphSerializer::ClassID,spRenderNodeSerializer::ClassID,"spNavigationGraphSerializer",&spRenderNodeSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spNavigationGraphSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spNavigationGraphSerializer::vfunc_18() const noexcept{return Record;}
    bool spNavigationGraphSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        auto* graph=dynamic_cast<spNavigationGraph*>(&object);std::uint32_t start=0,remaining=0;
        if(!graph||!object.IsExactly(spNavigationGraph::ClassID)||!source.GetCurrentPosition(start)||
            !ReadRenderNodeFieldsForAnalysis(context,source,size,*graph,false,error)||!RemainingSection(source,start,size,remaining))
        {context.failed=true;if(error&&error->empty())*error="Missing NavigationGraph section";return false;}
        SectionCursor cursor(context,source,remaining,true,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID==0)
            {
                auto* set=dynamic_cast<spNavigationSet*>(ReadFieldReferenceForAnalysis(context,spNavigationSet::ClassID,source,*header,error));
                if(context.failed||!graph->AppendSetForAnalysis(set))return cursor.Fail("Navigation graph set is null or wrong type");
            }
            else if(header->fieldID==1)
            {
                auto* portal=dynamic_cast<spNavigationPortal*>(ReadFieldReferenceForAnalysis(context,spNavigationPortal::ClassID,source,*header,error));
                if(context.failed||!graph->AppendPortalForAnalysis(portal))return cursor.Fail("Navigation graph portal is null or wrong type");
            }
            else if(header->fieldID==2)
            {
                std::uint32_t count=0;
                if(!cursor.Read(count)||!count||count>256)return cursor.Fail("Navigation graph table size exceeds host bound or is zero");
                // Original repeated populated resize produced a double release
                // in a bounded probe. Do not replace it with a guessed safe resize.
                if(!graph->paths_.empty())return cursor.Fail("Navigation graph table replacement has unsupported native lifetime");
                graph->paths_.resize(count);for(auto& row:graph->paths_)row.resize(count);
            }
            else if(header->fieldID==3)
            {
                std::uint32_t sourceSet=0,destination=0;std::uint8_t next=0,count=0;
                if(header->payloadSize<10||!source.Read(sourceSet)||!source.Read(destination)||!source.Read(next)||!source.Read(count)||
                    header->payloadSize!=10u+count*2u||sourceSet>=graph->paths_.size()||destination>=graph->paths_[sourceSet].size())
                    return cursor.Fail("Navigation path row exceeds initialized table or field bounds");
                auto& path=graph->paths_[sourceSet][destination];path.nextPortal=next;path.alternatives.resize(count);
                for(auto& alternative:path.alternatives)if(!source.ReadData(alternative.data(),2))return cursor.Fail("Truncated navigation path alternative");
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip NavigationGraph field");
        }
        return false;
    }
}
