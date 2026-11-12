#include "spContactProvidedTangentQueries.h"
#include "Analysis/PC/spContactProvidedTangentAbi.h"
#include "Analysis/PC/spContactProvidedTangentMath.h"

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
    spContactProvidedTangentResultForAnalysis ComputeContactProvidedTangentForAnalysis(
        const spContactProvidedTangentStateForAnalysis& before,
        const spContactProvidedTangentInputForAnalysis input,
        spContactProvidedTangentStateForAnalysis& output,
        const spContactProvidedTangentProfileForAnalysis profile) noexcept
    {
        using Result=spContactProvidedTangentResultForAnalysis;
        using namespace evidence::pc;
        if(profile!=spContactProvidedTangentProfileForAnalysis::PcCapturedWarmProvided07E5FiniteNearest037F)
            return Result::UnqualifiedProfile;
        if(Read(before.contact,0)!=spContactConstraintPrimary)
            return Result::CapturedPrimaryNotAdmitted;
        const auto count=Read(before.contact,0x58),full40=Read(before.contact,0x40),flags=Read(before.contact,0x88);
        if((count!=2&&count!=3)||full40>1||Read(before.contact,0x4C)!=0
            ||Read(before.contact,0x70)!=0||flags==0||(flags&~spContactProvidedTangentFlagMask)!=0)
            return Result::FixedStateNotAdmitted;
        for(const auto offset:{0x48U,0x6CU,0x74U,0x78U,0x7CU,0x80U,0x84U})
            if(Read(before.contact,offset)==0)return Result::FixedStateNotAdmitted;
        if(input.managerRaw7C!=spContactProvidedTangentManager7C||input.managerRaw80!=spContactProvidedTangentManager80)
            return Result::ManagerProfileNotAdmitted;
        if(input.scalarArgumentWord<0x3E800000U||input.scalarArgumentWord>0x40800000U)
            return Result::OutOfDomain;
        std::array<std::uint32_t,3> point{},normal{},provided{};
        for(unsigned lane=0;lane!=3;++lane)
        {
            point[lane]=Read(before.contact,0xB4+lane*4);normal[lane]=Read(before.contact,0xC0+lane*4);provided[lane]=Read(before.contact,0xD8+lane*4);
            if(!Bounded(point[lane])||!Bounded(normal[lane])||!Bounded(provided[lane])||!Bounded(input.bodyPointWords[lane]))
                return Result::OutOfDomain;
        }
        const auto cc=Read(before.contact,0xCC),raw8C=Read(before.contact,0x8C),raw90=Read(before.contact,0x90);
        if(!Bounded(cc)||!Bounded(raw8C)||!Bounded(raw90))return Result::OutOfDomain;
        const auto values=contact_provided_tangent_math::Query(
            input.scalarArgumentWord,point,normal,input.bodyPointWords,cc,provided,full40==1);
        auto staged=before;
        Write(staged.contact,0x68,8);Write(staged.contact,0x60,values.first.reciprocal);
        Write(staged.contact,0x64,input.managerRaw7C);
        // Actual warm-base loop initializes every selected row; unselected
        // slots and all padding are copied unchanged from the supplied value.
        for(unsigned row=0;row!=count;++row)
        {
            staged.raw78[row]=input.managerRaw80;staged.raw7C[row]=0xFF7FFFFFU;
            staged.raw80[row]=0x7F7FFFFFU;staged.raw84[row]=0xFFFFFFFFU;
        }
        staged.raw6C[0]=values.normal[0];staged.raw6C[1]=values.normal[1];staged.raw6C[2]=values.normal[2];
        staged.raw6C[6]=values.first.angular[2];staged.raw6C[4]=values.first.angular[0];staged.raw6C[5]=values.first.angular[1];
        staged.raw74[0]=values.first.rhs;staged.raw7C[0]=0;staged.raw80[0]=0x7F7FFFFFU;
        if(flags&4)staged.raw78[0]=Read(before.contact,0xA0);
        for(unsigned row=1;row!=count;++row)
        {
            const auto at=row*spContactProvidedTangentRowStrideWords;const auto source=row-1;
            for(unsigned lane=0;lane!=3;++lane)staged.raw6C[at+lane]=values.tangent[source][lane];
            staged.raw6C[at+6]=values.angular[source][2];
            staged.raw6C[at+4]=values.angular[source][0];staged.raw6C[at+5]=values.angular[source][1];
            const auto limit=row==2&&(flags&1)?raw90:raw8C;
            staged.raw7C[row]=contact_provided_tangent_math::StoreWord(
                sphere_contact_math::Negate(contact_provided_tangent_math::FromWord(limit)));
            staged.raw80[row]=limit;
            // Opaque DWORD overrides are copied without floating point admission.
            if(flags&(row==1?0x20U:0x40U))staged.raw74[row]=Read(before.contact,row==1?0xA4:0xA8);
            if(flags&(row==1?0x80U:0x100U))staged.raw78[row]=Read(before.contact,row==1?0xAC:0xB0);
            if(flags&(row==1?0x200U:0x400U))staged.raw84[row]=0;
        }
        output=staged;return Result::Completed;
    }
}
