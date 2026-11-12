#pragma once
#include "spContactRowMath.h"
#include "spContactTangentMath.h"

namespace sparkplug::evidence::pc::contact_multirow_math
{
    // Composition of existing kernels only; no new nearest64/root kernel.
    namespace arithmetic=sphere_contact_math;
    using contact_row_math::FromWord;
    using contact_row_math::StoreWord;
    struct Values final
    {
        contact_row_math::Values first;
        std::array<std::array<std::uint32_t,3>,2> tangent{},angular{};
        std::uint32_t negativeRaw8C=0;
    };
    inline Values Query(const std::uint32_t scalar,
        const std::array<std::uint32_t,3>& point,
        const std::array<std::uint32_t,3>& normal,
        const std::array<std::uint32_t,3>& bodyPoint,
        const std::uint32_t cc,const std::uint32_t raw8C) noexcept
    {
        Values result;
        result.first=contact_row_math::Query(scalar,point,normal,bodyPoint,cc);
        reconstruction::spContactTangentsInputForAnalysis input;
        for(unsigned lane=0;lane!=3;++lane)
            std::memcpy(&input.direction[lane],&normal[lane],4);
        reconstruction::spContactTangentsForAnalysis stored;
        contact_tangent_math::Query(input,stored);
        const auto dx=FromWord(result.first.displacement[0]);
        const auto dy=FromWord(result.first.displacement[1]);
        const auto dz=FromWord(result.first.displacement[2]);
        for(unsigned row=0;row!=2;++row)
        {
            const auto& vector=row==0?stored.first:stored.second;
            for(unsigned lane=0;lane!=3;++lane)
                std::memcpy(&result.tangent[row][lane],&vector[lane],4);
            // Native tangent outputs are F32 stores on the caller stack.
            // Reload those stored words before each angular cross operation.
            const auto nx=FromWord(result.tangent[row][0]);
            const auto ny=FromWord(result.tangent[row][1]);
            const auto nz=FromWord(result.tangent[row][2]);
            const auto x=arithmetic::Subtract(arithmetic::Multiply(dy,nz),arithmetic::Multiply(dz,ny));
            const auto y=arithmetic::Subtract(arithmetic::Multiply(dz,nx),arithmetic::Multiply(nz,dx));
            const auto z=arithmetic::Subtract(arithmetic::Multiply(ny,dx),arithmetic::Multiply(dy,nx));
            result.angular[row][2]=StoreWord(z);
            result.angular[row][0]=StoreWord(x);result.angular[row][1]=StoreWord(y);
        }
        result.negativeRaw8C=StoreWord(arithmetic::Negate(FromWord(raw8C)));
        return result;
    }
}
