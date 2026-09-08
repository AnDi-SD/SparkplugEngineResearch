#pragma once
// Inferred path. PC4879C0 ->13C7350/4FDF30; original allocation178.
// Represents serialized parameters and the needed CollisionInfo transform
// effects, not the native ABI or collision-query working storage.
#include "spBoundingVolume.h"
namespace sparkplug::reconstruction
{
    class spOBBBV final : public spBoundingVolume
    {
    public:
        static constexpr spClassID ClassID=0x4DA04889;
        spOBBBV() noexcept=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager& manager) const override;
        [[nodiscard]] const Vector3& GetPositionForAnalysis() const noexcept{return position_;}
        [[nodiscard]] const Vector3& GetSizeForAnalysis() const noexcept{return size_;}
        [[nodiscard]] const Matrix3& GetOrientationForAnalysis() const noexcept{return orientation_;}
        void SetPositionForAnalysis(const Vector3& value) noexcept{position_=boundingCenter_=value;}
        void SetSizeForAnalysis(const Vector3& value) noexcept;
        void SetOrientationForAnalysis(const Matrix3& value) noexcept{orientation_=value;}
        void UpdateCollisionTransformForAnalysis(Vector3& position,Matrix3& orientation,
            const Vector3& scale) const noexcept override;
    private:
        Vector3 position_{};
        Vector3 size_{1,1,1}; // ctor does not yet derive bounding sphere from size
        Matrix3 orientation_{1,0,0,0,1,0,0,0,1};
    };
}
