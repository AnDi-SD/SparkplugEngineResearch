#include "spAnimationQuaternionSlerpPreparationPc.h"
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

        constexpr std::uint32_t SignBit = 0x80000000u;
        constexpr std::uint32_t MagnitudeMask = 0x7FFFFFFFu;
        constexpr std::uint32_t OneBits = 0x3F800000u;

        constexpr bool Admitted(std::uint32_t bits) noexcept
        {
            const auto magnitude = bits & MagnitudeMask;
            return magnitude == 0 || (0x2F800000u <= magnitude && magnitude <= 0x45800000u);
        }

        math::Number Widen(std::uint32_t bits) noexcept
        {
            float scalar;
            std::memcpy(&scalar, &bits, sizeof(scalar));
            return math::From(scalar);
        }
    }

    bool TryPrepareAnimationQuaternionSlerpPcForAnalysis(
        const AnimationQuaternionPcWordsForAnalysis& first,
        const AnimationQuaternionPcWordsForAnalysis& second,
        AnimationQuaternionSlerpPreparationPcForAnalysis& output) noexcept
    {
        if (!std::all_of(first.begin(), first.end(), Admitted) ||
            !std::all_of(second.begin(), second.end(), Admitted))
            return false;

        const auto term = [&](std::size_t axis) noexcept
        {
            return math::Multiply(Widen(first[axis]), Widen(second[axis]));
        };
        const auto dot = math::Store(math::Add(math::Add(math::Add(term(2), term(1)), term(0)), term(3)));
        std::uint32_t dotBits;
        std::memcpy(&dotBits, &dot, sizeof(dotBits));

        auto adjusted = second;
        if (dotBits > SignBit)
        {
            dotBits ^= SignBit;
            for (auto& component : adjusted)
                component ^= SignBit;
        }
        if ((dotBits & MagnitudeMask) > OneBits)
            dotBits = OneBits;
        output = {dotBits, adjusted};
        return true;
    }
}
