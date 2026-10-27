#pragma once
#include "spProjection.h"

namespace sparkplug::reconstruction
{
    // Inferred portable declaration. Native PC factory41A1B0/table6DE868
    // establishes this physical Projection descendant; native extentD0.
    class spPyramidProjection : public spProjection
    {
    public:
        static constexpr spClassID ClassID=0x1CCA7732;
        using Vector3=std::array<float,3>;
        // Typed borrowed view of native parameter block9C, not its byte ABI.
        struct SerializedFieldsForAnalysis final
        {
            std::uint8_t& flip9C;
            Vector3& centerA0;
            Vector3& sizeAC;
        };
        struct OwnStateForAnalysis final
        {
            std::uint8_t flip9C=0;
            std::uint32_t wordB8=0;
            std::uint8_t byteCC=1;
        };
        spPyramidProjection();
        explicit spPyramidProjection(spProjectionConstructorDataForAnalysis);
        ~spPyramidProjection() override=default;
        static const spRTTIRecord& StaticRTTI() noexcept;
        const spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        const Vector3& GetCenterForAnalysis() const noexcept{return centerA0_;}
        const Vector3& GetSizeForAnalysis() const noexcept{return sizeAC_;}
        // Actual4331B0/slot58 returns this+9C. Each reader stage reacquires
        // this view after its successful read, before publishing that field.
        virtual SerializedFieldsForAnalysis GetSerializedFieldsForAnalysis() noexcept;
        const OwnStateForAnalysis& GetPyramidStateForAnalysis() const noexcept{return own_;}
        OwnStateForAnalysis& PyramidStateForFixture() noexcept{return own_;}
        // Existing data, not a recovered setter. Finite integer center/width/
        // depth in[-1024,1024], including signedzero. Height is a nonzero
        // signed power of two with magnitude<=1024. Refuses before mutation.
        bool SetGeometryForFixture(const Vector3& center,const Vector3& size) noexcept;
    protected:
        void RefreshProjectionSphereForAnalysis() const noexcept override;
        void ReadProjectionBoundsForAnalysis(BoundsPosition&,BoundsPosition&) const noexcept override;
    private:
        OwnStateForAnalysis own_{};
        Vector3 centerA0_{};
        Vector3 sizeAC_{1,1,1};
        // Native9D..9F, BC..CB and CD..CF remain unassigned/unmodeled.
    };
}
