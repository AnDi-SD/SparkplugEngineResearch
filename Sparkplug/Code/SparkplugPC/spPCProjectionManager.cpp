#include "spPCProjectionManager.h"
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPCProjectionManager>();}
        const spRTTIRecord record{spPCProjectionManager::ClassID,spProjectionManager::ClassID,
            "spPCProjectionManager",&spProjectionManager::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    const spRTTIRecord& spPCProjectionManager::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& spPCProjectionManager::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> spPCProjectionManager::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spPCProjectionManager>();
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
}
