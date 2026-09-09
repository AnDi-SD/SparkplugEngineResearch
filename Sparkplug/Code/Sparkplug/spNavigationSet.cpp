#include "spNavigationSet.h"
#include "spNavigationPortal.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord Record{spNavigationSet::ClassID,spNode::ClassID,"spNavigationSet",&spNode::StaticRTTI(),nullptr,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spNavigationSet::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spNavigationSet::vfunc_18() const noexcept{return Record;}
    bool spNavigationSet::SetNodeCountForAnalysis(std::uint32_t count)
    {
        if(count>4096)return false; // host allocation ceiling, not a native format limit
        nodeCount_=count;neighbours_.assign(count,{});return true;
    }
    bool spNavigationSet::AppendNeighbourForAnalysis(std::uint8_t source,std::uint8_t destination)
    {
        // Reader447AC0 calls this with reciprocal=false. It retains duplicates
        // and stores a byte degree; reject overflow at the host boundary.
        if(source>=neighbours_.size()||neighbours_[source].size()>=255)return false;
        neighbours_[source].push_back(destination);return true;
    }
    bool spNavigationSet::AppendPortalForAnalysis(spNavigationPortal* portal)
    {
        if(!portal||portals_.size()>=65536)return false;
        portals_.push_back(portal);return true;
    }
}
