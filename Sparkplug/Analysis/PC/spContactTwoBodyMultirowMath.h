#pragma once
#include "spContactTwoBodyRowMath.h"
#include "spContactTangentMath.h"

namespace sparkplug::evidence::pc::contact_two_body_multirow_math
{
    // Existing Number/RowMath/TangentMath composition; no new FP kernel.
    namespace arithmetic=sphere_contact_math;
    using contact_row_math::FromWord;
    using contact_row_math::StoreWord;
    struct Values final
    {
        contact_two_body_row_math::Values primary;
        std::array<std::array<std::uint32_t,3>,2> tangent{},angular1{},angular2{};
        std::uint32_t negativeRaw8C=0;
    };
    inline std::array<arithmetic::Number,3> Cross(
        const std::array<arithmetic::Number,3>& delta,
        const std::array<std::uint32_t,3>& vector) noexcept
    {
        const auto x=delta[0],y=delta[1],z=delta[2];
        const auto nx=FromWord(vector[0]),ny=FromWord(vector[1]),nz=FromWord(vector[2]);
        return {arithmetic::Subtract(arithmetic::Multiply(y,nz),arithmetic::Multiply(z,ny)),
                arithmetic::Subtract(arithmetic::Multiply(z,nx),arithmetic::Multiply(nz,x)),
                arithmetic::Subtract(arithmetic::Multiply(ny,x),arithmetic::Multiply(y,nx))};
    }
    inline Values Query(const std::uint32_t scalar,
        const std::array<std::uint32_t,3>& point,const std::array<std::uint32_t,3>& normal,
        const std::array<std::uint32_t,3>& bodyPoint,const std::array<std::uint32_t,3>& body2Point,
        const std::uint32_t cc,const std::uint32_t raw8C) noexcept
    {
        Values result;
        // Preserve primary J2 X-F32/YZ-extended and XY-spill/Z-extended
        // negation through the exact unchanged separately bounded helper.
        result.primary=contact_two_body_row_math::Query(scalar,point,normal,bodyPoint,body2Point,cc);
        reconstruction::spContactTangentsInputForAnalysis input;
        for(unsigned lane=0;lane!=3;++lane)std::memcpy(&input.direction[lane],&normal[lane],4);
        reconstruction::spContactTangentsForAnalysis stored;
        contact_tangent_math::Query(input,stored);
        std::array<arithmetic::Number,3> delta1{},delta2{};
        for(unsigned lane=0;lane!=3;++lane)
        {
            std::memcpy(&result.tangent[0][lane],&stored.first[lane],4);
            std::memcpy(&result.tangent[1][lane],&stored.second[lane],4);
            delta1[lane]=FromWord(result.primary.first.displacement[lane]);
            // Additional J2 rows reread stack58/5C/60: all three F32.
            delta2[lane]=FromWord(StoreWord(arithmetic::Subtract(FromWord(point[lane]),FromWord(body2Point[lane]))));
        }
        for(unsigned row=0;row!=2;++row)
        {
            const auto first=Cross(delta1,result.tangent[row]);
            result.angular1[row][2]=StoreWord(first[2]);
            result.angular1[row][0]=StoreWord(first[0]);result.angular1[row][1]=StoreWord(first[1]);
            // Original quirk: BOTH additional J2 rows use positive T2.
            const auto second=Cross(delta2,result.tangent[1]);
            const auto yStored=StoreWord(second[1]),zStored=StoreWord(second[2]);
            // X remains extended through FCHS; Y/Z are F32 then FCHS.
            const auto xNeg=arithmetic::Negate(second[0]);
            const auto yNeg=arithmetic::Negate(FromWord(yStored));
            const auto zNeg=arithmetic::Negate(FromWord(zStored));
            result.angular2[row][2]=StoreWord(zNeg);
            result.angular2[row][0]=StoreWord(xNeg);result.angular2[row][1]=StoreWord(yNeg);
        }
        result.negativeRaw8C=StoreWord(arithmetic::Negate(FromWord(raw8C)));
        return result;
    }
}
