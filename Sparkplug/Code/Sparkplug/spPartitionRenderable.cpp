#include "spPartitionRenderable.h"
#include "Code/SparkplugPC/spPCPartitionRenderable.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPCPartitionRenderable>();}
        const spRTTIRecord Record{spPartitionRenderable::ClassID,spBaseObject::ClassID,"spPartitionRenderable",&spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spPartitionRenderable::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spPartitionRenderable::vfunc_18() const noexcept{return Record;}
}
