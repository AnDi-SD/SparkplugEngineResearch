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
    void spBoundingVolume::ApplySimplePositionForAnalysis(const Vector3& localPosition,
        Vector3& position,const Matrix3& orientation) noexcept
    {
        // PC4723B7..4723FA: original order is z,y,x. X/Y remain wide until
        // adding the supplied position; Z has a distinct float temporary.
        // Finite scalar reconstruction, not a universal x87 emulator.
        double offset[3];
        for(std::size_t c=0;c<3;++c)
            offset[c]=(double(localPosition[2])*orientation[6+c]
                +double(localPosition[1])*orientation[3+c])+double(localPosition[0])*orientation[c];
        const float z=static_cast<float>(offset[2]);
        position[0]=static_cast<float>(offset[0]+position[0]);
        position[1]=static_cast<float>(offset[1]+position[1]);
        position[2]=z+position[2];
    }
    bool spBoundingVolume::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* target=dynamic_cast<spBoundingVolume*>(&destination);
        if(!target||!spBaseObject::vfunc_14(destination,manager))return false;
        // Inherited PC413120 copies the physical name, not geometry/bounds.
        CopyNameToForAnalysis(*target);return true;
    }
}
