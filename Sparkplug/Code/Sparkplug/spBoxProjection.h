#pragma once
#include "spProjection.h"

namespace sparkplug::reconstruction
{
    class spBoxProjectionSerializer;
    class spBoxProjection final : public spProjection
    {
    public:
        static constexpr spClassID ClassID=0x32BB2F56;
        using Vector3=std::array<float,3>;
        // RTTI factory form requires explicitly bound common constructor data.
        spBoxProjection();
        explicit spBoxProjection(spProjectionConstructorDataForAnalysis);
        ~spBoxProjection() override=default;
        static const spRTTIRecord& StaticRTTI() noexcept;
        const spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        const Vector3& GetCenterForAnalysis() const noexcept{return center9C_;}
        const Vector3& GetSizeForAnalysis() const noexcept{return sizeA8_;}
        // Existing input data, not an original setter/producer claim. Exact
        // small integers and signedzero in[-1024,1024] only. Invalid input is
        // refused before mutation; native arbitrary floats/FP modes are open.
        bool SetGeometryForFixture(const Vector3& center,const Vector3& size) noexcept;
    protected:
        void RefreshProjectionSphereForAnalysis() const noexcept override;
        void ReadProjectionBoundsForAnalysis(BoundsPosition&,BoundsPosition&) const noexcept override;
    private:
        friend class spBoxProjectionSerializer;
        Vector3 center9C_{};
        Vector3 sizeA8_{1,1,1};
        // Native+B4 is unassigned by the constructor and remains unmodeled.
    };
}
