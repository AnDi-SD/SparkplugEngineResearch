#pragma once
#include "spContactRowMath.h"

namespace sparkplug::evidence::pc::contact_two_body_row_math
{
    namespace arithmetic = sphere_contact_math;
    using contact_row_math::FromWord;
    using contact_row_math::StoreWord;
    struct Values final
    {
        contact_row_math::Values first;
        std::array<std::uint32_t,3> linear2{},angular2{};
    };
    inline Values Query(const std::uint32_t scalar,
        const std::array<std::uint32_t,3>& point,
        const std::array<std::uint32_t,3>& normal,
        const std::array<std::uint32_t,3>& bodyPoint,
        const std::array<std::uint32_t,3>& body2Point,
        const std::uint32_t cc) noexcept
    {
        Values output;
        // Body1/base/RHS reuse the exact unchanged admitted implementation.
        output.first=contact_row_math::Query(scalar,point,normal,bodyPoint,cc);
        const auto rx=FromWord(StoreWord(arithmetic::Subtract(FromWord(point[0]),FromWord(body2Point[0]))));
        // Native FST copies Y/Z to F32, but leaves extended ST values alive.
        // Those unused copies must not replace the arithmetic operands.
        const auto ry=arithmetic::Subtract(FromWord(point[1]),FromWord(body2Point[1]));
        const auto rz=arithmetic::Subtract(FromWord(point[2]),FromWord(body2Point[2]));
        const auto nx=FromWord(normal[0]),ny=FromWord(normal[1]),nz=FromWord(normal[2]);
        const auto x=arithmetic::Subtract(arithmetic::Multiply(ry,nz),arithmetic::Multiply(rz,ny));
        const auto y=arithmetic::Subtract(arithmetic::Multiply(rz,nx),arithmetic::Multiply(nz,rx));
        const auto z=arithmetic::Subtract(arithmetic::Multiply(ny,rx),arithmetic::Multiply(ry,nx));
        for(unsigned lane=0;lane!=3;++lane)output.linear2[lane]=StoreWord(arithmetic::Negate(FromWord(normal[lane])));
        // AngularX/Y are spilled to F32 before FCHS. AngularZ is negated
        // while extended, then stored. Preserve both sign and rounding order.
        output.angular2[0]=StoreWord(arithmetic::Negate(FromWord(StoreWord(x))));
        output.angular2[1]=StoreWord(arithmetic::Negate(FromWord(StoreWord(y))));
        output.angular2[2]=StoreWord(arithmetic::Negate(z));
        return output;
    }
}
