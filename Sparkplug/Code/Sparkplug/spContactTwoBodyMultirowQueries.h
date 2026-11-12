#pragma once
#include "spContactConstraint.h"
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // Separate portable projection:228 captured bytes +96 raw6C +96 raw70 bytes +5x12.
    // Tags in captured pointer fields are inert words, never dereferenced.
    struct spContactTwoBodyMultirowStateForAnalysis final
    {
        spContactConstraint::CapturedStateForAnalysis contact{};
        std::array<std::uint32_t,24> raw6C{},raw70{};
        std::array<std::uint32_t,3> raw74{},raw78{},raw7C{},raw80{},raw84{};
    };
    struct spContactTwoBodyMultirowInputForAnalysis final
    {
        std::uint32_t scalarArgumentWord=0;
        std::array<std::uint32_t,3> bodyPointWords{},body2PointWords{};
        std::uint32_t managerRaw7C=0,managerRaw80=0;
    };
    enum class spContactTwoBodyMultirowProfileForAnalysis
    { Unqualified,PcCapturedWarmFiniteNearest037F };
    enum class spContactTwoBodyMultirowResultForAnalysis
    { Completed,UnqualifiedProfile,CapturedPrimaryNotAdmitted,FixedStateNotAdmitted,
      ManagerProfileNotAdmitted,OutOfDomain };

    // Analytical value operation only: count58=2/3,full40/88=0,nonzero distinct48/4C and6C/70.
    // Other required role tags nonzero; exact supplied Manager words; scalar
    // positive[.25,4], point/normal/bothBodyPoints/rawCC/raw8C finite abs<=16,
    // selected tangent pair nonzero. Default/refusal preserves output.
    // Staging permits before/output alias. No native class/base/caller ABI,
    // singleton access, carrier owner, lifetime or solver interface is implied.
    spContactTwoBodyMultirowResultForAnalysis ComputeContactTwoBodyMultirowForAnalysis(
        const spContactTwoBodyMultirowStateForAnalysis& before,
        spContactTwoBodyMultirowInputForAnalysis input,
        spContactTwoBodyMultirowStateForAnalysis& output,
        spContactTwoBodyMultirowProfileForAnalysis profile=
            spContactTwoBodyMultirowProfileForAnalysis::Unqualified) noexcept;
}
