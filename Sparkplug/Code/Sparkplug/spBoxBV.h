#pragma once
// Inferred path. PC registration6D4640, factory488310, allocationC4.
// Only authored shape, base bounds, clone and CollisionInfo transform effects
// needed by tools are represented; native query buffers are outside this slice.
#include "spBoundingVolume.h"

namespace sparkplug::reconstruction
{
    class spBoxBV final : public spBoundingVolume
    {
    public:
        static constexpr spClassID ClassID=0x7B4C0876;
        spBoxBV() noexcept=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager& manager) const override;
        [[nodiscard]] const Vector3& GetPositionForAnalysis() const noexcept{return position_;}
        [[nodiscard]] const Vector3& GetSizeForAnalysis() const noexcept{return size_;}
        void SetPositionForAnalysis(const Vector3& value) noexcept{position_=boundingCenter_=value;}
        void SetSizeForAnalysis(const Vector3& value) noexcept;
        void UpdateCollisionTransformForAnalysis(Vector3& position,Matrix3& orientation,
            const Vector3& scale) const noexcept override;
    private:
        Vector3 position_{};
        Vector3 size_{1,1,1}; // Original ctor leaves the base radius zero.
    };
}
