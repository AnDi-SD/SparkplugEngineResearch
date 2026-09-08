#include "spBoundingVolume.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord Record{spBoundingVolume::ClassID,spBaseObject::ClassID,"spBoundingVolume",
            &spBaseObject::StaticRTTI(),nullptr,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spBoundingVolume::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spBoundingVolume::vfunc_18() const noexcept{return Record;}
    bool spBoundingVolume::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* target=dynamic_cast<spBoundingVolume*>(&destination);
        if(!target||!spBaseObject::vfunc_14(destination,manager))return false;
        // Inherited PC413120 copies the physical name, not geometry/bounds.
        CopyNameToForAnalysis(*target);return true;
    }
}
