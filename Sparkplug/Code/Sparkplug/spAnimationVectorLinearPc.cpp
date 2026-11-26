#include "spAnimationVectorLinearPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        constexpr std::uint32_t MinimumMagnitude = 0x2F800000u;
        constexpr std::uint32_t MaximumCoordinateMagnitude = 0x45800000u;
        constexpr std::uint32_t OneBits = 0x3F800000u;
        constexpr std::uint32_t MagnitudeMask = 0x7FFFFFFFu;

        bool AdmittedCoordinate(std::uint32_t bits) noexcept
        {
            const auto magnitude = bits & MagnitudeMask;
            return magnitude == 0 || (magnitude >= MinimumMagnitude && magnitude <= MaximumCoordinateMagnitude);
        }

        bool AdmittedFactor(std::uint32_t bits) noexcept
        {
            return (bits & MagnitudeMask) == 0 || (bits >= MinimumMagnitude && bits <= OneBits);
        }

        math::Number Widen(std::uint32_t bits) noexcept
        {
            float scalar;
            std::memcpy(&scalar, &bits, sizeof(scalar));
            return math::From(scalar);
        }

        std::uint32_t StoreWord(math::Number value) noexcept
        {
            const auto scalar = math::Store(value);
            std::uint32_t bits;
            std::memcpy(&bits, &scalar, sizeof(bits));
            return bits;
        }
    }

    bool TryInterpolateAnimationVectorLinearPcForAnalysis(
        const AnimationVectorLinearPcWordsForAnalysis& first,
        const AnimationVectorLinearPcWordsForAnalysis& second,
        std::uint32_t factorBits,
        AnimationVectorLinearPcWordsForAnalysis& output) noexcept
    {
        if (!AdmittedFactor(factorBits) ||
            !std::all_of(first.begin(), first.end(), AdmittedCoordinate) ||
            !std::all_of(second.begin(), second.end(), AdmittedCoordinate))
            return false;

        const auto factor = Widen(factorBits);
        const auto complement = math::Subtract(Widen(OneBits), factor);
        AnimationVectorLinearPcWordsForAnalysis result{};
        result[0] = StoreWord(math::Add(math::Multiply(complement, Widen(first[0])),
                                        math::Multiply(factor, Widen(second[0]))));
        // Native X keeps both products in x87. Y and Z spill each product to
        // binary32 before the sum. Delay all output writes for input aliases.
        for (std::size_t axis = 1; axis < result.size(); ++axis)
        {
            const auto left = Widen(StoreWord(math::Multiply(complement, Widen(first[axis]))));
            const auto right = Widen(StoreWord(math::Multiply(factor, Widen(second[axis]))));
            result[axis] = StoreWord(math::Add(left, right));
        }
        output = result;
        return true;
    }
}
