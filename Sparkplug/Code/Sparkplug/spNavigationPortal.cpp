#include "spNavigationPortal.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spNavigationPortal>();}
        const spRTTIRecord Record{spNavigationPortal::ClassID,spNode::ClassID,"spNavigationPortal",&spNode::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spNavigationPortal::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spNavigationPortal::vfunc_18() const noexcept{return Record;}
}
