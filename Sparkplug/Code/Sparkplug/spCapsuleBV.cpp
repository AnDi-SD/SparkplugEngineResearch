#include "spCapsuleBV.h"
#include "../../Analysis/PC/spCapsuleBVMath.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spCapsuleBV>();}
        const spRTTIRecord Record{spCapsuleBV::ClassID,spBoundingVolume::ClassID,"spCapsuleBV",
            &spBoundingVolume::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spCapsuleBV::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spCapsuleBV::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spCapsuleBV::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spCapsuleBV>();manager.RegisterClone(*this,*clone);
        // PC488530 invokes inherited Named copy413120 through native slot0C:
        // authored shape and endpoint cache remain factory defaults.
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    spCapsuleBV::StateForAnalysis spCapsuleBV::GetStateForAnalysis() const noexcept
    {return {length_,radius_,orientation_,first_,second_,position_};}
    void spCapsuleBV::SetStateForAnalysis(const StateForAnalysis& state) noexcept
    {
        length_=state.length;radius_=state.radius;orientation_=state.orientation;
        first_=state.first;second_=state.second;position_=state.position;
    }
    bool spCapsuleBV::TryUpdateCollisionTransformForAnalysis(Vector3& position,
        Matrix3& orientation,const Vector3& scale) const noexcept
    {
        // Radius+2C is not read by native488A20; incoming scale is unchanged.
        return evidence::pc::capsule_bv_math::Transform(position_,orientation_,length_,
            position,orientation,scale,first_,second_);
    }
    void spCapsuleBV::UpdateCollisionTransformForAnalysis(Vector3& position,
        Matrix3& orientation,const Vector3& scale) const noexcept
    {(void)TryUpdateCollisionTransformForAnalysis(position,orientation,scale);}
}
