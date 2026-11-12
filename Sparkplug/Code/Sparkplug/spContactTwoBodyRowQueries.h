#pragma once
#include "spContactConstraint.h"
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // Our value views of one original two-body Contact row. Buffer names are native
    // field offsets, not recovered solver types or a portable pointer ABI.
    struct spContactTwoBodyRowStateForAnalysis final
    {
        spContactConstraint::CapturedStateForAnalysis contact{};
        std::array<std::uint32_t, 24> raw6C{}, raw70{};
        std::array<std::uint32_t, 3> raw74{}, raw78{}, raw7C{}, raw80{}, raw84{};
    };
    struct spContactTwoBodyRowInputForAnalysis final
    {
        std::uint32_t scalarArgumentWord = 0;
        std::array<std::uint32_t, 3> bodyPointWords{}, body2PointWords{};
        // Explicit supplied values from the captured132B Manager profile.
        // No Manager object, singleton, constructor or pointer is accessed.
        std::uint32_t managerRaw7C = 0;
        std::uint32_t managerRaw80 = 0;
    };
    enum class spContactTwoBodyRowProfileForAnalysis
    { Unqualified, PcCapturedWarmFiniteNearest037F };
    enum class spContactTwoBodyRowResultForAnalysis
    { Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted, FixedStateNotAdmitted,
      ManagerProfileNotAdmitted, OutOfDomain };

    // Point/normal/CC are raw ContactB4/C0/CC values; bodyPoint is supplied
    // explicitly in place of the original unclassified read-only carrier.
    // Fixed profile:40=0,58=1,completeDWORD88=0; distinctnonzero48/4C and6C/70; required supplied
    // body1/body2/buffer role tags are nonzero inert words. Scalar is positive
    // [0.25,4], other numeric inputs finite abs<=16, Manager words exact.
    // Refusal preserves output. A staged result permits before/output alias.
    spContactTwoBodyRowResultForAnalysis ComputeContactTwoBodyRowForAnalysis(
        const spContactTwoBodyRowStateForAnalysis& before,
        spContactTwoBodyRowInputForAnalysis input,
        spContactTwoBodyRowStateForAnalysis& output,
        spContactTwoBodyRowProfileForAnalysis profile =
            spContactTwoBodyRowProfileForAnalysis::Unqualified) noexcept;
}
