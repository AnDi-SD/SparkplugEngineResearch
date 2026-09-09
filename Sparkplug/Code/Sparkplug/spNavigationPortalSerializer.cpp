#include "spNavigationPortalSerializer.h"
#include "spNavigationPortal.h"
#include "spNavigationGraph.h"
#include "spNavigationSet.h"
#include "Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction
{
    using namespace evidence::pc::serialization;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spNavigationPortalSerializer>();}
        const spRTTIRecord Record{spNavigationPortalSerializer::ClassID,spNodeSerializer::ClassID,"spNavigationPortalSerializer",&spNodeSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spNavigationPortalSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spNavigationPortalSerializer::vfunc_18() const noexcept{return Record;}
    bool spNavigationPortalSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        auto* portal=dynamic_cast<spNavigationPortal*>(&object);std::uint32_t start=0,remaining=0;
        if(!portal||!object.IsExactly(spNavigationPortal::ClassID)||!source.GetCurrentPosition(start)||
            !ReadNodeFieldsForAnalysis(context,source,size,*portal,false,error)||!RemainingSection(source,start,size,remaining))
        {context.failed=true;if(error&&error->empty())*error="Missing NavigationPortal section";return false;}
        SectionCursor cursor(context,source,remaining,true,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID==0)
            {
                auto* graph=dynamic_cast<spNavigationGraph*>(ReadFieldReferenceForAnalysis(context,spNavigationGraph::ClassID,source,*header,error));
                if(context.failed||!graph)return cursor.Fail("Navigation portal graph is null or wrong type");
                portal->graph_=graph;portal->graphKnown_=true;
            }
            else if(header->fieldID==1)
            {
                const auto end=header->dataStreamPosition+header->payloadSize;
                auto* first=dynamic_cast<spNavigationSet*>(ReadSequenceReferenceForAnalysis(context,spNavigationSet::ClassID,source,end,error));
                if(context.failed||!first)return cursor.Fail("Navigation portal first endpoint is null or wrong type");
                auto* second=dynamic_cast<spNavigationSet*>(ReadSequenceReferenceForAnalysis(context,spNavigationSet::ClassID,source,end,error));
                if(context.failed||!second)return cursor.Fail("Navigation portal second endpoint is null or wrong type");
                portal->endpoints_={first,second};portal->endpointsKnown_=true;
            }
            else if(header->fieldID==2)
            {
                std::array<std::uint8_t,2> pair{};
                if(!cursor.Read(pair)||portal->firstNodes_.size()>=65536)return cursor.Fail("Invalid navigation portal node pair");
                portal->firstNodes_.push_back(pair[0]);portal->secondNodes_.push_back(pair[1]);
            }
            else if(header->fieldID==3)
            {
                std::array<std::uint8_t,3> path{};
                if(!cursor.Read(path)||portal->paths_.size()>=65536)return cursor.Fail("Invalid navigation portal path membership");
                portal->paths_.push_back(path);
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip NavigationPortal field");
        }
        return false;
    }
}
