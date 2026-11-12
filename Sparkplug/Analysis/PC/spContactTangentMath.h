#pragma once
// PC48B220..48B2D6, two finite selected-pair branches. Historical Sphere
// arithmetic placement is retained: its explicit Number-root overload and
// existing signed-zero product/reciprocal serve two bounded value queries.
#include "Code/Sparkplug/spConstraintValueQueries.h"
#include "spSphereContactMath.h"

namespace sparkplug::evidence::pc::contact_tangent_math
{
    namespace arithmetic = sphere_contact_math;
    inline constexpr float Threshold = 0x1.6A09E6p-1F; // exact raw3F3504F3

    inline void Query(const reconstruction::spContactTangentsInputForAnalysis& input,
                      reconstruction::spContactTangentsForAnalysis& output) noexcept
    {
        const auto x = arithmetic::From(input.direction[0]);
        const auto y = arithmetic::From(input.direction[1]);
        const auto z = arithmetic::From(input.direction[2]);
        const bool yz = input.direction[2] > Threshold;
        if (yz) output.first[0] = 0.0F; // Original literal store precedes root/division.
        const auto firstSquare = arithmetic::Multiply(yz ? y : x, yz ? y : x);
        const auto secondSquare = arithmetic::Multiply(yz ? z : y, yz ? z : y);
        // No F32 spill: original FSQRT consumes the extended square sum.
        const auto squared = arithmetic::Add(firstSquare, secondSquare);
        const auto inverse = arithmetic::Reciprocal(arithmetic::SquareRoot(squared));
        if (yz)
        {
            output.first[1] = arithmetic::Store(arithmetic::Negate(arithmetic::Multiply(inverse, z)));
            output.first[2] = arithmetic::Store(arithmetic::Multiply(inverse, y));
            output.second[0] = arithmetic::Store(arithmetic::Multiply(inverse, squared));
            output.second[1] = arithmetic::Store(arithmetic::Negate(
                arithmetic::Multiply(arithmetic::From(output.first[2]), x)));
            output.second[2] = arithmetic::Store(arithmetic::Multiply(x, arithmetic::From(output.first[1])));
        }
        else
        {
            output.first[0] = arithmetic::Store(arithmetic::Negate(arithmetic::Multiply(inverse, y)));
            output.first[2] = 0.0F;
            output.first[1] = arithmetic::Store(arithmetic::Multiply(inverse, x));
            output.second[0] = arithmetic::Store(arithmetic::Negate(
                arithmetic::Multiply(arithmetic::From(output.first[1]), z)));
            output.second[1] = arithmetic::Store(arithmetic::Multiply(arithmetic::From(output.first[0]), z));
            output.second[2] = arithmetic::Store(arithmetic::Multiply(inverse, squared));
        }
    }
}
