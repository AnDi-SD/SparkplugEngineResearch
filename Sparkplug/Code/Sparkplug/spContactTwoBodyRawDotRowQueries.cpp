#include "spContactTwoBodyRawDotRowQueries.h"
#include "Analysis/PC/spContactTwoBodyRawDotRowAbi.h"
#include "Analysis/PC/spContactTwoBodyRawDotRowMath.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        using Captured = spContactConstraint::CapturedStateForAnalysis;
        std::uint32_t Read(const Captured& bytes, const std::size_t offset) noexcept
        {
            std::uint32_t word = 0;
            for (unsigned byte = 0; byte != 4; ++byte)
                word |= std::uint32_t{bytes[offset + byte]} << (byte * 8);
            return word;
        }
        void Write(Captured& bytes, const std::size_t offset, const std::uint32_t word) noexcept
        {
            for (unsigned byte = 0; byte != 4; ++byte)
                bytes[offset + byte] = static_cast<std::uint8_t>(word >> (byte * 8));
        }
        bool Bounded(const std::uint32_t word) noexcept
        { return (word & 0x7FFFFFFFU) <= 0x41800000U; }
    }

    spContactTwoBodyRawDotRowResultForAnalysis ComputeContactTwoBodyRawDotRowForAnalysis(
        const spContactTwoBodyRawDotRowStateForAnalysis& before,
        const spContactTwoBodyRawDotRowInputForAnalysis input,
        spContactTwoBodyRawDotRowStateForAnalysis& output,
        spContactTwoBodyRawDotRowDiagnosticForAnalysis& diagnostic,
        const spContactTwoBodyRawDotRowProfileForAnalysis profile) noexcept
    {
        using Result = spContactTwoBodyRawDotRowResultForAnalysis;
        using namespace evidence::pc;
        if (profile != spContactTwoBodyRawDotRowProfileForAnalysis::PcCapturedWarmFiniteRawDotNearest037F)
            return Result::UnqualifiedProfile;
        if (Read(before.contact, 0) != spContactConstraintPrimary)
            return Result::CapturedPrimaryNotAdmitted;
        if (Read(before.contact, 0x40) != 0
            || Read(before.contact, 0x58) != 1
            || Read(before.contact, 0x88) != 8)
            return Result::FixedStateNotAdmitted;
        for (const auto offset : {0x48U, 0x4CU, 0x6CU, 0x70U, 0x74U, 0x78U, 0x7CU, 0x80U, 0x84U})
            if (Read(before.contact, offset) == 0) return Result::FixedStateNotAdmitted;
        if (Read(before.contact,0x48)==Read(before.contact,0x4C) || Read(before.contact,0x6C)==Read(before.contact,0x70))
            return Result::FixedStateNotAdmitted;
        if (input.managerRaw7C != spContactTwoBodyRowManager7C
            || input.managerRaw80 != spContactTwoBodyRowManager80)
            return Result::ManagerProfileNotAdmitted;
        if (input.scalarArgumentWord < 0x3E800000U || input.scalarArgumentWord > 0x40800000U)
            return Result::OutOfDomain;
        std::array<std::uint32_t, 3> point{}, normal{};
        for (unsigned lane = 0; lane != 3; ++lane)
        {
            point[lane] = Read(before.contact, spContactTwoBodyRowPoint + lane * 4);
            normal[lane] = Read(before.contact, spContactTwoBodyRowNormal + lane * 4);
            if (!Bounded(point[lane]) || !Bounded(normal[lane]) || !Bounded(input.bodyPointWords[lane]) || !Bounded(input.body2PointWords[lane]))
                return Result::OutOfDomain;
        }
        const auto cc = Read(before.contact, spContactTwoBodyRowCC);
        const auto raw94=Read(before.contact,spContactTwoBodyRawDotMultiplier94);
        const auto raw98=Read(before.contact,spContactTwoBodyRawDotThreshold98);
        if(!Bounded(cc)||!Bounded(raw94)||!Bounded(raw98))return Result::OutOfDomain;
        for(const auto word:input.bodyRaw8CToA0Words)if(!Bounded(word))return Result::OutOfDomain;
        for(const auto word:input.body2Raw8CToA0Words)if(!Bounded(word))return Result::OutOfDomain;
        const auto values = contact_two_body_raw_dot_row_math::Query(input.scalarArgumentWord, point, normal, input.bodyPointWords, input.body2PointWords, cc, input.bodyRaw8CToA0Words, input.body2Raw8CToA0Words, raw94, raw98);
        auto staged = before;
        // Staged value projection. This is not a C++ machine-write trace
        // or a native ABI/lifetime guarantee.
        Write(staged.contact, 0x68, 8);
        Write(staged.contact, 0x60, values.primary.first.reciprocal);
        Write(staged.contact, 0x64, input.managerRaw7C);
        staged.raw78[0] = input.managerRaw80;
        staged.raw7C[0] = 0xFF7FFFFFU;
        staged.raw80[0] = 0x7F7FFFFFU;
        staged.raw84[0] = 0xFFFFFFFFU;
        staged.raw6C[0] = normal[0]; staged.raw6C[1] = normal[1]; staged.raw6C[2] = normal[2];
        staged.raw6C[6] = values.primary.first.angular[2];
        staged.raw6C[4] = values.primary.first.angular[0]; staged.raw6C[5] = values.primary.first.angular[1];
        staged.raw70[2]=values.primary.linear2[2]; staged.raw70[0]=values.primary.linear2[0]; staged.raw70[1]=values.primary.linear2[1];
        staged.raw70[6]=values.primary.angular2[2]; staged.raw70[4]=values.primary.angular2[0]; staged.raw70[5]=values.primary.angular2[1];
        staged.raw74[0] = values.primary.first.rhs;
        staged.raw7C[0] = 0;
        staged.raw80[0] = 0x7F7FFFFFU;
        if(values.rhsStoreSelected)staged.raw74[0]=values.rhs;
        const spContactTwoBodyRawDotRowDiagnosticForAnalysis stagedDiagnostic{values.rhsStoreSelected};
        output = staged;
        diagnostic = stagedDiagnostic;
        return Result::Completed;
    }
}
