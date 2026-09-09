#include "spBoxBV.h"
#include "spBoxBVSerializer.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spBoxBV>();}
        const spRTTIRecord Record{spBoxBV::ClassID,spBoundingVolume::ClassID,"spBoxBV",
            &spBoundingVolume::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spBoxBV::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spBoxBV::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spBoxBV::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spBoxBV>();manager.RegisterClone(*this,*clone);
        // PC488370 calls inherited413120; geometry remains ctor defaults.
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    void spBoxBV::SetSizeForAnalysis(const Vector3& value) noexcept
    {
        size_=value;
        boundingRadius_=spBoxBVSerializer::DecodeSizeForAnalysis({value[0],value[1],value[2]}).boundingSphereRadius;
    }
    void spBoxBV::UpdateCollisionTransformForAnalysis(Vector3& position,Matrix3& orientation,
        const Vector3&) const noexcept
    {
        ApplySimplePositionForAnalysis(position_,position,orientation);
    }
}
