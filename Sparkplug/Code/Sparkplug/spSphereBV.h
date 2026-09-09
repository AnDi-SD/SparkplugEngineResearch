#pragma once
// Inferred path. PC registration6D3C40, factory472450, allocation60.
// Partial actual class: authored shape, base bounds, clone and the transform
// side effect needed by CollisionInfo; no collision-query/storage ABI claim.
#include "spBoundingVolume.h"

namespace sparkplug::reconstruction
{
    class spSphereBV final : public spBoundingVolume
    {
    public:
        static constexpr spClassID ClassID=0x390946D2;
        spSphereBV() noexcept=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager& manager) const override;
        [[nodiscard]] const Vector3& GetPositionForAnalysis() const noexcept{return position_;}
        [[nodiscard]] float GetRadiusForAnalysis() const noexcept{return radius_;}
        void SetPositionForAnalysis(const Vector3& value) noexcept{position_=boundingCenter_=value;}
        void SetRadiusForAnalysis(float value) noexcept{radius_=boundingRadius_=value;}
        void UpdateCollisionTransformForAnalysis(Vector3& position,Matrix3& orientation,
            const Vector3& scale) const noexcept override;
    private:
        Vector3 position_{};
        float radius_=0;
    };
}
