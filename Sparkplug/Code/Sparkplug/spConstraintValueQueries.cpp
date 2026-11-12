#include "spConstraintValueQueries.h"
#include "Analysis/PC/spContactTangentMath.h"
#include <cmath>

namespace sparkplug::reconstruction
{
    spContactTangentsResultForAnalysis ComputeContactTangentsForAnalysis(
        spContactTangentsInputForAnalysis input, spContactTangentsForAnalysis& output,
        const spContactTangentsProfileForAnalysis profile) noexcept
    {
        using Result = spContactTangentsResultForAnalysis;
        if (profile != spContactTangentsProfileForAnalysis::PcFiniteNearest037F)
            return Result::UnqualifiedProfile;
        for (const auto value : input.direction)
            if (!std::isfinite(value) || std::fabs(value) > 256.0F)
                return Result::OutOfDomain;
        // The original tests signed Z, not its absolute value. In the XY
        // branch, both selected zero components would divide by zero.
        if (!(input.direction[2] > evidence::pc::contact_tangent_math::Threshold)
            && input.direction[0] == 0.0F && input.direction[1] == 0.0F)
            return Result::OutOfDomain;
        evidence::pc::contact_tangent_math::Query(input, output);
        return Result::Completed;
    }
}
