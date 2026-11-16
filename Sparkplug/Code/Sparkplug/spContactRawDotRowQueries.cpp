#include "spContactRawDotRowQueries.h"
#include "Analysis/PC/spContactRawDotRowAbi.h"
#include "Analysis/PC/spContactRawDotRowMath.h"

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

    spContactRawDotRowResultForAnalysis ComputeContactRawDotRowForAnalysis(
        const spContactRawDotRowStateForAnalysis& before,
        const spContactRawDotRowInputForAnalysis input,
        spContactRawDotRowStateForAnalysis& output,
        spContactRawDotRowDiagnosticForAnalysis& diagnostic,
        const spContactRawDotRowProfileForAnalysis profile) noexcept
    {
        using Result = spContactRawDotRowResultForAnalysis;
        using namespace evidence::pc;
        if (profile != spContactRawDotRowProfileForAnalysis::PcCapturedWarmFiniteRawDotNearest037F)
            return Result::UnqualifiedProfile;
        if (Read(before.contact, 0) != spContactConstraintPrimary)
            return Result::CapturedPrimaryNotAdmitted;
        if (Read(before.contact, 0x40) != 0 || Read(before.contact, 0x4C) != 0
            || Read(before.contact, 0x58) != 1 || Read(before.contact, 0x70) != 0
            || Read(before.contact, 0x88) != 8)
            return Result::FixedStateNotAdmitted;
        for (const auto offset : {0x48U, 0x6CU, 0x74U, 0x78U, 0x7CU, 0x80U, 0x84U})
            if (Read(before.contact, offset) == 0) return Result::FixedStateNotAdmitted;
        if (input.managerRaw7C != spContactSingleBodyRowManager7C
            || input.managerRaw80 != spContactSingleBodyRowManager80)
            return Result::ManagerProfileNotAdmitted;
        if (input.scalarArgumentWord < 0x3E800000U || input.scalarArgumentWord > 0x40800000U)
            return Result::OutOfDomain;
        std::array<std::uint32_t, 3> point{}, normal{};
        for (unsigned lane = 0; lane != 3; ++lane)
        {
            point[lane] = Read(before.contact, spContactSingleBodyRowPoint + lane * 4);
            normal[lane] = Read(before.contact, spContactSingleBodyRowNormal + lane * 4);
            if (!Bounded(point[lane]) || !Bounded(normal[lane]) || !Bounded(input.bodyPointWords[lane]))
                return Result::OutOfDomain;
        }
        const auto cc = Read(before.contact, spContactSingleBodyRowCC);
        const auto raw94=Read(before.contact, spContactRawDotRowMultiplier94);
        const auto raw98=Read(before.contact, spContactRawDotRowThreshold98);
        if (!Bounded(cc) || !Bounded(raw94) || !Bounded(raw98)) return Result::OutOfDomain;
        for(const auto word:input.bodyRaw8CToA0Words)
            if(!Bounded(word))return Result::OutOfDomain;
        const auto values = contact_raw_dot_row_math::Query(input.scalarArgumentWord, point, normal, input.bodyPointWords, cc, input.bodyRaw8CToA0Words, raw94, raw98);
        auto staged = before;
        // Project into a private staged value. Compiler machine-write order
        // is not a native store trace or an ABI guarantee.
        Write(staged.contact, 0x68, 8);
        Write(staged.contact, 0x60, values.first.reciprocal);
        Write(staged.contact, 0x64, input.managerRaw7C);
        staged.raw78[0] = input.managerRaw80;
        staged.raw7C[0] = 0xFF7FFFFFU;
        staged.raw80[0] = 0x7F7FFFFFU;
        staged.raw84[0] = 0xFFFFFFFFU;
        staged.raw6C[0] = normal[0]; staged.raw6C[1] = normal[1]; staged.raw6C[2] = normal[2];
        staged.raw6C[6] = values.first.angular[2];
        staged.raw6C[4] = values.first.angular[0]; staged.raw6C[5] = values.first.angular[1];
        staged.raw74[0] = values.first.rhs;
        staged.raw7C[0] = 0;
        staged.raw80[0] = 0x7F7FFFFFU;
        if(values.rhsStoreSelected)staged.raw74[0]=values.rhs;
        const spContactRawDotRowDiagnosticForAnalysis stagedDiagnostic{values.rhsStoreSelected};
        output = staged;
        diagnostic = stagedDiagnostic;
        return Result::Completed;
    }
}
