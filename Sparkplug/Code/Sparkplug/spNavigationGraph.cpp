#include "spNavigationGraph.h"
#include "spNavigationSet.h"
#include "spNavigationPortal.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spNavigationGraph>();}
        const spRTTIRecord Record{spNavigationGraph::ClassID,spRenderNode::ClassID,"spNavigationGraph",&spRenderNode::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spNavigationGraph::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spNavigationGraph::vfunc_18() const noexcept{return Record;}
    bool spNavigationGraph::AppendSetForAnalysis(spNavigationSet* value)
    {
        if(!value||sets_.size()>=65536)return false;
        value->SetGraphIndexForAnalysis(static_cast<std::uint8_t>(sets_.size()));sets_.push_back(value);return true;
    }
    bool spNavigationGraph::AppendPortalForAnalysis(spNavigationPortal* value)
    {
        if(!value||portals_.size()>=65536)return false;
        value->SetGraphIndexForAnalysis(static_cast<std::uint8_t>(portals_.size()));portals_.push_back(value);return true;
    }
}
