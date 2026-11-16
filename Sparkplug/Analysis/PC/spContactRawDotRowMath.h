#pragma once
#include "spContactRowMath.h"

namespace sparkplug::evidence::pc::contact_raw_dot_row_math
{
    namespace arithmetic = sphere_contact_math;
    using contact_row_math::FromWord;
    using contact_row_math::StoreWord;
    struct Values final
    {
        contact_row_math::Values first;
        std::uint32_t rhs = 0;
        bool rhsStoreSelected = false;
    };
    inline Values Query(const std::uint32_t scalar,
        const std::array<std::uint32_t,3>& point,
        const std::array<std::uint32_t,3>& normal,
        const std::array<std::uint32_t,3>& bodyPoint,
        const std::uint32_t cc,
        const std::array<std::uint32_t,6>& body,
        const std::uint32_t raw94, const std::uint32_t raw98) noexcept
    {
        Values output;
        output.first=contact_row_math::Query(scalar,point,normal,bodyPoint,cc);
        output.rhs=output.first.rhs;
        // Original order, including reread F32 angular words and nearest64
        // after each product/add. Do not reassociate as an exact dot sum.
        auto dot=arithmetic::Multiply(FromWord(body[5]),FromWord(output.first.angular[2]));
        dot=arithmetic::Add(dot,arithmetic::Multiply(FromWord(body[4]),FromWord(output.first.angular[1])));
        dot=arithmetic::Add(dot,arithmetic::Multiply(FromWord(body[2]),FromWord(normal[2])));
        dot=arithmetic::Add(dot,arithmetic::Multiply(FromWord(body[1]),FromWord(normal[1])));
        dot=arithmetic::Add(dot,arithmetic::Multiply(FromWord(body[3]),FromWord(output.first.angular[0])));
        dot=arithmetic::Add(dot,arithmetic::Multiply(FromWord(body[0]),FromWord(normal[0])));
        const auto threshold=FromWord(raw98);
        if(arithmetic::nearest::Compare(threshold,FromWord(0))<0)return output;
        if(arithmetic::nearest::Compare(arithmetic::Negate(dot),threshold)<=0)return output;
        const auto candidate=arithmetic::Negate(arithmetic::Multiply(dot,FromWord(raw94)));
        // Compare while extended; a positive2^-298 candidate must select
        // the original store even when StoreWord later produces +0 again.
        if(arithmetic::nearest::Compare(candidate,FromWord(output.first.rhs))>0)
        {
            output.rhsStoreSelected=true;
            output.rhs=StoreWord(candidate);
        }
        return output;
    }
}
