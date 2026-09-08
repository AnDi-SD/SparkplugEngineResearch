#include "spOBBBV.h"
#include "spOBBBVSerializer.h"
#include "Analysis/PC/spNodeTransformMath.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spOBBBV>();}
        const spRTTIRecord Record{spOBBBV::ClassID,spBoundingVolume::ClassID,"spOBBBV",
            &spBoundingVolume::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spOBBBV::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spOBBBV::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spOBBBV::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spOBBBV>();manager.RegisterClone(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    void spOBBBV::SetSizeForAnalysis(const Vector3& value) noexcept
    {
        size_=value;
        boundingRadius_=spOBBBVSerializer::DecodeSizeForAnalysis({value[0],value[1],value[2]}).boundingSphereRadius;
    }
    void spOBBBV::UpdateCollisionTransformForAnalysis(Vector3& position,Matrix3& orientation,
        const Vector3&) const noexcept
    {
        // PC486930..486A19: modifies the supplied CollisionInfo PR, does NOT
        // modify the serialized local OBB parameters or multiply by scale.
        const auto offset=evidence::pc::node_math::Transform(position_,orientation);
        for(std::size_t i=0;i<3;++i)position[i]+=offset[i];
        orientation=evidence::pc::node_math::Multiply(orientation_,orientation);
    }
}
