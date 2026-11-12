#pragma once
#include "spContactConstraint.h"
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // Separate portable projection:228 captured bytes +96 raw6C bytes +5x12.
    // Tags in captured pointer fields are inert words, never dereferenced.
    struct spContactProvidedTangentStateForAnalysis final
    {
        spContactConstraint::CapturedStateForAnalysis contact{};
        std::array<std::uint32_t,24> raw6C{};
        std::array<std::uint32_t,3> raw74{},raw78{},raw7C{},raw80{},raw84{};
    };
    struct spContactProvidedTangentInputForAnalysis final
    {
        std::uint32_t scalarArgumentWord=0;
        std::array<std::uint32_t,3> bodyPointWords{};
        std::uint32_t managerRaw7C=0,managerRaw80=0;
    };
    enum class spContactProvidedTangentProfileForAnalysis
    { Unqualified,PcCapturedWarmProvided07E5FiniteNearest037F };
    enum class spContactProvidedTangentResultForAnalysis
    { Completed,UnqualifiedProfile,CapturedPrimaryNotAdmitted,FixedStateNotAdmitted,
      ManagerProfileNotAdmitted,OutOfDomain };

    // Count2/3,full40=0/1,full88 nonzero subset07E5,4C/70=0.
    // Scalar[.25,4]; point/normal/bodyPoint/CC/8C/90/providedD8..E0 finite abs<=16.
    // Zero vectors allowed; no FSQRT/tangenthelper. A0/A4/A8/AC/B0 opaqueDWORDs.
    // Exact suppliedManager; default/refusal preserves output; staging permits alias.
    // No native class/base/caller-return ABI/carrier/lifetime or solver claim.
    spContactProvidedTangentResultForAnalysis ComputeContactProvidedTangentForAnalysis(
        const spContactProvidedTangentStateForAnalysis& before,
        spContactProvidedTangentInputForAnalysis input,
        spContactProvidedTangentStateForAnalysis& output,
        spContactProvidedTangentProfileForAnalysis profile=
            spContactProvidedTangentProfileForAnalysis::Unqualified) noexcept;
}
