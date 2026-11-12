#pragma once
#include "spContactConstraint.h"
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // Separate portable projection:228 captured bytes +96 raw6C bytes +5x12.
    // Tags in captured pointer fields are inert words, never dereferenced.
    struct spContactMultirowStateForAnalysis final
    {
        spContactConstraint::CapturedStateForAnalysis contact{};
        std::array<std::uint32_t,24> raw6C{};
        std::array<std::uint32_t,3> raw74{},raw78{},raw7C{},raw80{},raw84{};
    };
    struct spContactMultirowInputForAnalysis final
    {
        std::uint32_t scalarArgumentWord=0;
        std::array<std::uint32_t,3> bodyPointWords{};
        std::uint32_t managerRaw7C=0,managerRaw80=0;
    };
    enum class spContactMultirowProfileForAnalysis
    { Unqualified,PcCapturedWarmFiniteNearest037F };
    enum class spContactMultirowResultForAnalysis
    { Completed,UnqualifiedProfile,CapturedPrimaryNotAdmitted,FixedStateNotAdmitted,
      ManagerProfileNotAdmitted,OutOfDomain };

    // Analytical value operation only: count58=2/3,full40/88=0,4C/70=0.
    // Other required role tags nonzero; exact supplied Manager words; scalar
    // positive[.25,4], point/normal/bodyPoint/rawCC/raw8C finite abs<=16,
    // selected tangent pair nonzero. Default/refusal preserves output.
    // Staging permits before/output alias. No native class/base/caller ABI,
    // singleton access, carrier owner, lifetime or solver interface is implied.
    spContactMultirowResultForAnalysis ComputeContactMultirowForAnalysis(
        const spContactMultirowStateForAnalysis& before,
        spContactMultirowInputForAnalysis input,
        spContactMultirowStateForAnalysis& output,
        spContactMultirowProfileForAnalysis profile=
            spContactMultirowProfileForAnalysis::Unqualified) noexcept;
}
