#include "spPCPartitionRenderable.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPCPartitionRenderable>();}
        const spRTTIRecord Record{spPCPartitionRenderable::ClassID,spPartitionRenderable::ClassID,"spPCPartitionRenderable",&spPartitionRenderable::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spPCPartitionRenderable::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spPCPartitionRenderable::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spPCPartitionRenderable::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spPCPartitionRenderable>();manager.RegisterClone(*this,*clone);
        return spBaseObject::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
}
