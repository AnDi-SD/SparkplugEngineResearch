#pragma once
// Inferred path. PC491E50 ->416510 constructs a physical named prefix;
// PC6D48DD/PS248038C register spBaseObject as the direct RTTI base.
#include "Code/SparkBase/spBaseObject.h"
#include <array>

namespace sparkplug::reconstruction
{
    class spBoundingVolume : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID=0x21CC76AF;
        using Vector3=std::array<float,3>;
        using Matrix3=std::array<float,9>;
        ~spBoundingVolume() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        bool vfunc_14(spBaseObject& destination,spCloneManager& manager) const override;
        [[nodiscard]] const Vector3& GetBoundingCenterForAnalysis() const noexcept{return boundingCenter_;}
        [[nodiscard]] float GetBoundingRadiusForAnalysis() const noexcept{return boundingRadius_;}

        // The transform effects of native BV slot1C on CollisionInfo. Query
        // planes, contacts and broadphase caches are outside this tools slice;
        // this API does not expose a collision/intersection query operation.
        virtual void UpdateCollisionTransformForAnalysis(Vector3& position,Matrix3& orientation,
            const Vector3& scale) const noexcept=0;
    protected:
        spBoundingVolume() noexcept=default;
        // Shared PC4723B0 in Sphere/Box slot1C; named from this tools slice.
        // Other BV subclasses retain their own original transform behavior.
        static void ApplySimplePositionForAnalysis(const Vector3& localPosition,
            Vector3& position,const Matrix3& orientation) noexcept;
        Vector3 boundingCenter_{};
        float boundingRadius_=0;
    };
}
