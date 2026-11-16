#pragma once
#include "spContactTwoBodyRowMath.h"
namespace sparkplug::evidence::pc::contact_two_body_raw_dot_row_math
{
    namespace arithmetic=sphere_contact_math;
    using contact_row_math::FromWord;
    using contact_row_math::StoreWord;
    // One composer for both native partial sums. The pinned SingleRawDot
    // draft exposes only full Query, so cannot supply extended D1 directly.
    inline arithmetic::Number OrderedDot(const std::array<std::uint32_t,6>& body,
        const std::array<std::uint32_t,3>& linear,
        const std::array<std::uint32_t,3>& angular,
        const bool fifthAngularFirst) noexcept
    {
        auto dot=arithmetic::Multiply(FromWord(body[5]),FromWord(angular[2]));
        dot=arithmetic::Add(dot,arithmetic::Multiply(FromWord(body[4]),FromWord(angular[1])));
        dot=arithmetic::Add(dot,arithmetic::Multiply(FromWord(body[2]),FromWord(linear[2])));
        dot=arithmetic::Add(dot,arithmetic::Multiply(FromWord(body[1]),FromWord(linear[1])));
        const auto fifth=fifthAngularFirst
            ? arithmetic::Multiply(FromWord(angular[0]),FromWord(body[3]))
            : arithmetic::Multiply(FromWord(body[3]),FromWord(angular[0]));
        dot=arithmetic::Add(dot,fifth);
        dot=arithmetic::Add(dot,arithmetic::Multiply(FromWord(body[0]),FromWord(linear[0])));
        return dot;
    }
    struct Values final
    {
        contact_two_body_row_math::Values primary;
        std::uint32_t rhs=0;
        bool rhsStoreSelected=false;
    };
    inline void SelectRhs(const arithmetic::Number combined,
        const std::uint32_t raw94,const std::uint32_t raw98,Values& output) noexcept
    {
        const auto threshold=FromWord(raw98);
        if(arithmetic::nearest::Compare(threshold,FromWord(0))<0)return;
        if(arithmetic::nearest::Compare(arithmetic::Negate(combined),threshold)<=0)return;
        const auto candidate=arithmetic::Negate(arithmetic::Multiply(combined,FromWord(raw94)));
        if(arithmetic::nearest::Compare(candidate,FromWord(output.primary.first.rhs))>0)
        {
            // PC0048B8E6 is selected while extended, even if the following
            // F32 conversion produces the current RHS word again.
            output.rhsStoreSelected=true;
            output.rhs=StoreWord(candidate);
        }
    }
    inline Values Query(const std::uint32_t scalar,
        const std::array<std::uint32_t,3>& point,const std::array<std::uint32_t,3>& normal,
        const std::array<std::uint32_t,3>& bodyPoint,const std::array<std::uint32_t,3>& body2Point,
        const std::uint32_t cc,const std::array<std::uint32_t,6>& body,
        const std::array<std::uint32_t,6>& body2,const std::uint32_t raw94,const std::uint32_t raw98) noexcept
    {
        Values output;
        // Existing primary helper preserves body2 X spill/YZ extended and
        // angularXY F32-before-FCHS/Z FCHS-before-F32 without any new kernel.
        output.primary=contact_two_body_row_math::Query(scalar,point,normal,bodyPoint,body2Point,cc);
        output.rhs=output.primary.first.rhs;
        const auto one=OrderedDot(body,normal,output.primary.first.angular,false);
        const auto two=OrderedDot(body2,output.primary.linear2,output.primary.angular2,true);
        // Original48B8AC: one nearest64 addition AFTER the two ordered sums.
        const auto combined=arithmetic::Add(one,two);
        SelectRhs(combined,raw94,raw98,output);
        return output;
    }
}
