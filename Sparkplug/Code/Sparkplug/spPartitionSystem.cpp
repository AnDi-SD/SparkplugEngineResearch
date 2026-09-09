#include "spPartitionSystem.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPartitionSystem>();}
        const spRTTIRecord Record{spPartitionSystem::ClassID,spNode::ClassID,"spPartitionSystem",&spNode::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    spPartitionSystem::spPartitionSystem() noexcept{SetStaticForAnalysis(true);}
    spPartitionSystem::~spPartitionSystem()=default;
    const spRTTIRecord& spPartitionSystem::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spPartitionSystem::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spPartitionSystem::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spPartitionSystem>();manager.RegisterClone(*this,*clone);
        return spRenderNode::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spPartitionSystem::SetPartitionRootForAnalysis(std::unique_ptr<spPartitionNode> root) noexcept
    {
        if(!root||root_)return false;
        root_=std::move(root);return true;
    }
}
