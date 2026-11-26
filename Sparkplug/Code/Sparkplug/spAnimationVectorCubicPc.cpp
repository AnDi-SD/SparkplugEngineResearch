#include "spAnimationVectorCubicPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);
        static_assert(sizeof(AnimationVectorCubicPcRecordForAnalysis) == 60);

        constexpr std::uint32_t MinimumMagnitude = 0x2F800000u;
        constexpr std::uint32_t MaximumMagnitude = 0x45800000u;
        constexpr std::uint32_t OneBits = 0x3F800000u;
        constexpr std::uint32_t MagnitudeMask = 0x7FFFFFFFu;

        bool AdmittedVector(const AnimationVectorCubicPcWordsForAnalysis& words) noexcept
        {
            return std::all_of(words.begin(), words.end(), [](std::uint32_t bits) {
                const auto magnitude = bits & MagnitudeMask;
                return magnitude == 0 || (magnitude >= MinimumMagnitude && magnitude <= MaximumMagnitude);
            });
        }

        bool AdmittedFactor(std::uint32_t bits) noexcept
        {
            return (bits & MagnitudeMask) == 0 || (bits >= MinimumMagnitude && bits <= OneBits);
        }

        math::Number Widen(std::uint32_t bits) noexcept
        {
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return math::From(value);
        }

        math::Number Spill(math::Number value) noexcept
        {
            return math::From(math::Store(value));
        }

        std::uint32_t StoreWord(math::Number value) noexcept
        {
            const auto scalar = math::Store(value);
            std::uint32_t bits;
            std::memcpy(&bits, &scalar, sizeof(bits));
            return bits;
        }
    }

    bool TrySampleAnimationVectorCubicPcForAnalysis(
        const AnimationVectorCubicPcRecordForAnalysis& record,
        std::uint32_t factorBits,
        AnimationVectorCubicPcWordsForAnalysis& output) noexcept
    {
        if (!AdmittedFactor(factorBits) || !AdmittedVector(record.value) ||
            !AdmittedVector(record.outgoingTangent) || !AdmittedVector(record.quadratic) ||
            !AdmittedVector(record.cubic))
            return false;

        const auto t = Widen(factorBits);

        // X spills the quadratic sum and its product. The last product stays in x87.
        const auto xQuadratic = Spill(math::Add(math::Multiply(t, Widen(record.cubic[0])), Widen(record.quadratic[0])));
        const auto xOutgoing = math::Add(Spill(math::Multiply(xQuadratic, t)), Widen(record.outgoingTangent[0]));
        const auto x = math::Add(math::Multiply(xOutgoing, t), Widen(record.value[0]));

        // Y preserves the quadratic sum and spills all three subsequent stages.
        const auto yQuadratic = math::Add(math::Multiply(t, Widen(record.cubic[1])), Widen(record.quadratic[1]));
        const auto yOutgoing = Spill(math::Add(Spill(math::Multiply(yQuadratic, t)), Widen(record.outgoingTangent[1])));
        const auto y = math::Add(Spill(math::Multiply(yOutgoing, t)), Widen(record.value[1]));

        // Z spills the first product, then the outgoing sum and final product.
        const auto zQuadratic = math::Add(Spill(math::Multiply(t, Widen(record.cubic[2]))), Widen(record.quadratic[2]));
        const auto zOutgoing = Spill(math::Add(math::Multiply(zQuadratic, t), Widen(record.outgoingTangent[2])));
        const auto z = math::Add(Spill(math::Multiply(zOutgoing, t)), Widen(record.value[2]));

        output = {StoreWord(x), StoreWord(y), StoreWord(z)};
        return true;
    }
}
