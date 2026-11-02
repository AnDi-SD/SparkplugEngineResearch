#include "spBoundingVolumeQueries.h"
#include "Analysis/PC/spSphereContactMath.h"
#include <cmath>

namespace sparkplug::reconstruction
{
    spSphereContactResultForAnalysis ComputeSphereContactForAnalysis(
        spSphereContactInputForAnalysis input, spSphereContactForAnalysis& output,
        const spSphereContactProfileForAnalysis profile) noexcept
    {
        using Result = spSphereContactResultForAnalysis;
        if (profile != spSphereContactProfileForAnalysis::PcFiniteNearest037F)
            return Result::UnqualifiedProfile;
        const auto admitted = [](float value) noexcept
        { return std::isfinite(value) && std::fabs(value) <= 256.0F; };
        for (unsigned lane = 0; lane != 3; ++lane)
            if (!admitted(input.center1[lane]) || !admitted(input.center2[lane]))
                return Result::OutOfDomain;
        if (!admitted(input.radius1) || !admitted(input.radius2))
            return Result::OutOfDomain;
        return evidence::pc::sphere_contact_math::Query(input, output) ? Result::Contact : Result::NoContact;
    }
}
