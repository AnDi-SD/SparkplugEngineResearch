#include "spContactTwoBodyMultirowQueries.h"
#include "Analysis/PC/spContactTwoBodyMultirowAbi.h"
#include "Analysis/PC/spContactTwoBodyMultirowMath.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        using Captured=spContactConstraint::CapturedStateForAnalysis;
        std::uint32_t Read(const Captured& bytes,const std::size_t offset) noexcept
        {
            std::uint32_t word=0;
            for(unsigned b=0;b!=4;++b)word|=std::uint32_t{bytes[offset+b]}<<(8*b);
            return word;
        }
        void Write(Captured& bytes,const std::size_t offset,const std::uint32_t word) noexcept
        { for(unsigned b=0;b!=4;++b)bytes[offset+b]=static_cast<std::uint8_t>(word>>(8*b)); }
        bool Bounded(const std::uint32_t word) noexcept
        { return (word&0x7FFFFFFFU)<=0x41800000U; }
    }
    spContactTwoBodyMultirowResultForAnalysis ComputeContactTwoBodyMultirowForAnalysis(
        const spContactTwoBodyMultirowStateForAnalysis& before,
        const spContactTwoBodyMultirowInputForAnalysis input,
        spContactTwoBodyMultirowStateForAnalysis& output,
        const spContactTwoBodyMultirowProfileForAnalysis profile) noexcept
    {
        using Result=spContactTwoBodyMultirowResultForAnalysis;
        using namespace evidence::pc;
        if(profile!=spContactTwoBodyMultirowProfileForAnalysis::PcCapturedWarmFiniteNearest037F)
            return Result::UnqualifiedProfile;
        if(Read(before.contact,0)!=spContactConstraintPrimary)
            return Result::CapturedPrimaryNotAdmitted;
        const auto count=Read(before.contact,0x58);
        if((count!=2&&count!=3)||Read(before.contact,0x40)!=0||Read(before.contact,0x88)!=0)
            return Result::FixedStateNotAdmitted;
        for(const auto offset:{0x48U,0x4CU,0x6CU,0x70U,0x74U,0x78U,0x7CU,0x80U,0x84U})
            if(Read(before.contact,offset)==0)return Result::FixedStateNotAdmitted;
        if(Read(before.contact,0x48)==Read(before.contact,0x4C)||Read(before.contact,0x6C)==Read(before.contact,0x70))return Result::FixedStateNotAdmitted;
        if(input.managerRaw7C!=spContactTwoBodyMultirowManager7C||input.managerRaw80!=spContactTwoBodyMultirowManager80)
            return Result::ManagerProfileNotAdmitted;
        if(input.scalarArgumentWord<0x3E800000U||input.scalarArgumentWord>0x40800000U)
            return Result::OutOfDomain;
        std::array<std::uint32_t,3> point{},normal{};
        for(unsigned lane=0;lane!=3;++lane)
        {
            point[lane]=Read(before.contact,0xB4+lane*4);normal[lane]=Read(before.contact,0xC0+lane*4);
            if(!Bounded(point[lane])||!Bounded(normal[lane])||!Bounded(input.bodyPointWords[lane])||!Bounded(input.body2PointWords[lane]))
                return Result::OutOfDomain;
        }
        const auto cc=Read(before.contact,0xCC),raw8C=Read(before.contact,0x8C);
        if(!Bounded(cc)||!Bounded(raw8C))return Result::OutOfDomain;
        const bool yz=(normal[2]&0x80000000U)==0&&normal[2]>0x3F3504F3U;
        const auto a=yz?normal[1]:normal[0],b=yz?normal[2]:normal[1];
        if(((a|b)&0x7FFFFFFFU)==0)return Result::OutOfDomain;
        const auto values=contact_two_body_multirow_math::Query(input.scalarArgumentWord,point,normal,input.bodyPointWords,input.body2PointWords,cc,raw8C);
        auto staged=before;
        Write(staged.contact,0x68,8);Write(staged.contact,0x60,values.primary.first.reciprocal);
        Write(staged.contact,0x64,input.managerRaw7C);
        // Actual warm-base loop initializes every selected row; unselected
        // slots and all padding are copied unchanged from the supplied value.
        for(unsigned row=0;row!=count;++row)
        {
            staged.raw78[row]=input.managerRaw80;staged.raw7C[row]=0xFF7FFFFFU;
            staged.raw80[row]=0x7F7FFFFFU;staged.raw84[row]=0xFFFFFFFFU;
        }
        staged.raw6C[0]=normal[0];staged.raw6C[1]=normal[1];staged.raw6C[2]=normal[2];
        staged.raw6C[6]=values.primary.first.angular[2];staged.raw6C[4]=values.primary.first.angular[0];staged.raw6C[5]=values.primary.first.angular[1];
        for(unsigned lane=0;lane!=3;++lane)staged.raw70[lane]=values.primary.linear2[lane];
        staged.raw70[6]=values.primary.angular2[2];staged.raw70[4]=values.primary.angular2[0];staged.raw70[5]=values.primary.angular2[1];
        staged.raw74[0]=values.primary.first.rhs;staged.raw7C[0]=0;staged.raw80[0]=0x7F7FFFFFU;
        for(unsigned row=1;row!=count;++row)
        {
            const auto at=row*spContactTwoBodyMultirowRowStrideWords;const auto source=row-1;
            for(unsigned lane=0;lane!=3;++lane)staged.raw6C[at+lane]=values.tangent[source][lane];
            staged.raw6C[at+6]=values.angular1[source][2];
            staged.raw6C[at+4]=values.angular1[source][0];staged.raw6C[at+5]=values.angular1[source][1];
            // Both additional J2 linear rows copy positive T2.
            for(unsigned lane=0;lane!=3;++lane)staged.raw70[at+lane]=values.tangent[1][lane];
            staged.raw70[at+6]=values.angular2[source][2];
            staged.raw70[at+4]=values.angular2[source][0];staged.raw70[at+5]=values.angular2[source][1];
            staged.raw7C[row]=values.negativeRaw8C;staged.raw80[row]=raw8C;
            // Complete DWORD88=0 leaves raw74[row] untouched, even if dirty.
        }
        output=staged;return Result::Completed;
    }
}
