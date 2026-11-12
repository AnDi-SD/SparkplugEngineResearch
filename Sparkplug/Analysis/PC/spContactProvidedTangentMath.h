#pragma once
#include "spContactRowMath.h"

namespace sparkplug::evidence::pc::contact_provided_tangent_math
{
    // Compose unchanged Number/RowMath operations; no new kernel or FSQRT.
    namespace arithmetic=sphere_contact_math;
    using contact_row_math::FromWord;
    using contact_row_math::StoreWord;
    inline std::array<std::uint32_t,3> CrossStored(
        const std::array<std::uint32_t,3>& a,const std::array<std::uint32_t,3>& b) noexcept
    {
        const auto ax=FromWord(a[0]),ay=FromWord(a[1]),az=FromWord(a[2]);
        const auto bx=FromWord(b[0]),by=FromWord(b[1]),bz=FromWord(b[2]);
        const auto x=arithmetic::Subtract(arithmetic::Multiply(ay,bz),arithmetic::Multiply(az,by));
        const auto y=arithmetic::Subtract(arithmetic::Multiply(az,bx),arithmetic::Multiply(bz,ax));
        const auto z=arithmetic::Subtract(arithmetic::Multiply(by,ax),arithmetic::Multiply(ay,bx));
        std::array<std::uint32_t,3> out{};
        // Native spill/reload Z as F32, then store X and Y.
        out[2]=StoreWord(z);out[0]=StoreWord(x);out[1]=StoreWord(y);return out;
    }
    struct Values final
    {
        contact_row_math::Values first;
        std::array<std::uint32_t,3> normal{};
        std::array<std::array<std::uint32_t,3>,2> tangent{},angular{};
    };
    inline Values Query(const std::uint32_t scalar,
        const std::array<std::uint32_t,3>& point,const std::array<std::uint32_t,3>& normal,
        const std::array<std::uint32_t,3>& bodyPoint,const std::uint32_t cc,
        const std::array<std::uint32_t,3>& provided,const bool flip) noexcept
    {
        Values out;
        for(unsigned lane=0;lane!=3;++lane)
            out.normal[lane]=flip?StoreWord(arithmetic::Negate(FromWord(normal[lane]))):normal[lane];
        out.first=contact_row_math::Query(scalar,point,out.normal,bodyPoint,cc);
        out.tangent[0]=provided;out.tangent[1]=CrossStored(out.normal,provided);
        // Reload stored F32 tangent2 before its angular cross. Displacement
        // words likewise come from the original F32 spill/reload.
        out.angular[0]=CrossStored(out.first.displacement,out.tangent[0]);
        out.angular[1]=CrossStored(out.first.displacement,out.tangent[1]);return out;
    }
}
