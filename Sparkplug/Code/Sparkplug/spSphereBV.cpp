#include "spSphereBV.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spSphereBV>();}
        const spRTTIRecord Record{spSphereBV::ClassID,spBoundingVolume::ClassID,"spSphereBV",
            &spBoundingVolume::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spSphereBV::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spSphereBV::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spSphereBV::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spSphereBV>();manager.RegisterClone(*this,*clone);
        // PC4724B0 calls inherited413120: only the named prefix is copied.
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    void spSphereBV::UpdateCollisionTransformForAnalysis(Vector3& position,Matrix3& orientation,
        const Vector3&) const noexcept
    {
        ApplySimplePositionForAnalysis(position_,position,orientation);
    }
}
