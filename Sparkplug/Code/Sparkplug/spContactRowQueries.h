#pragma once
#include "spContactConstraint.h"
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // Our value views of one original Contact row. Buffer names are native
    // field offsets, not recovered solver types or a portable pointer ABI.
    struct spContactSingleBodyRowStateForAnalysis final
    {
        spContactConstraint::CapturedStateForAnalysis contact{};
        std::array<std::uint32_t, 24> raw6C{};
        std::array<std::uint32_t, 3> raw74{}, raw78{}, raw7C{}, raw80{}, raw84{};
    };
    struct spContactSingleBodyRowInputForAnalysis final
    {
        std::uint32_t scalarArgumentWord = 0;
        std::array<std::uint32_t, 3> bodyPointWords{};
        // Explicit supplied values from the captured132B Manager profile.
        // No Manager object, singleton, constructor or pointer is accessed.
        std::uint32_t managerRaw7C = 0;
        std::uint32_t managerRaw80 = 0;
    };
    enum class spContactSingleBodyRowProfileForAnalysis
    { Unqualified, PcCapturedWarmFiniteNearest037F };
    enum class spContactSingleBodyRowResultForAnalysis
    { Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted, FixedStateNotAdmitted,
      ManagerProfileNotAdmitted, OutOfDomain };

    // Point/normal/CC are raw ContactB4/C0/CC values; bodyPoint is supplied
    // explicitly in place of the original unclassified read-only carrier.
    // Fixed profile:40=0,4C=0,58=1,70=0,completeDWORD88=0; required supplied
    // body1/buffer role tags are nonzero inert words. Scalar is positive
    // [0.25,4], other numeric inputs finite abs<=16, Manager words exact.
    // Refusal preserves output. A staged result permits before/output alias.
    spContactSingleBodyRowResultForAnalysis ComputeContactSingleBodyRowForAnalysis(
        const spContactSingleBodyRowStateForAnalysis& before,
        spContactSingleBodyRowInputForAnalysis input,
        spContactSingleBodyRowStateForAnalysis& output,
        spContactSingleBodyRowProfileForAnalysis profile =
            spContactSingleBodyRowProfileForAnalysis::Unqualified) noexcept;
}
