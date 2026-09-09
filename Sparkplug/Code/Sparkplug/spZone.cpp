#include "spZone.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spZone>();}
        const spRTTIRecord Record{spZone::ClassID,spNode::ClassID,"spZone",&spNode::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spZone::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spZone::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spZone::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spZone>();manager.RegisterClone(*this,*clone);
        return spNode::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spZone::AppendRootForAnalysis(spPartitionNode* root)
    {
        if(roots_.size()>=4096)return false; // host allocation bound only
        roots_.push_back(root);return true;
    }
}
