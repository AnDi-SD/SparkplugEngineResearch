#pragma once
#include <array>

namespace sparkplug::reconstruction
{
    // Portable value views; original declaration owner of PC48B220 is unknown.
    struct spContactTangentsInputForAnalysis final
    {
        std::array<float, 3> direction{};
    };
    struct spContactTangentsForAnalysis final
    {
        std::array<float, 3> first{};
        std::array<float, 3> second{};
    };
    enum class spContactTangentsProfileForAnalysis { Unqualified, PcFiniteNearest037F };
    enum class spContactTangentsResultForAnalysis { Completed, UnqualifiedProfile, OutOfDomain };

    // Our free-function API for the bounded native value operation. Input is
    // copied; refusal preserves both outputs. The admitted profile requires
    // finite |input|<=256 and a positive selected two-component square sum.
    spContactTangentsResultForAnalysis ComputeContactTangentsForAnalysis(
        spContactTangentsInputForAnalysis input, spContactTangentsForAnalysis& output,
        spContactTangentsProfileForAnalysis profile = spContactTangentsProfileForAnalysis::Unqualified) noexcept;
}
