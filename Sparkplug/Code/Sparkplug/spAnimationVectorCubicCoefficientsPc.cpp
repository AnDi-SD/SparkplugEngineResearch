#include "spAnimationVectorCubicCoefficientsPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;
        using Record = AnimationVectorCubicPcRecordForAnalysis;
        using Words = AnimationVectorCubicPcWordsForAnalysis;

        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);
        static_assert(sizeof(Record) == 60);
        static_assert(offsetof(Record, value) == 0);
        static_assert(offsetof(Record, incomingTangent) == 12);
        static_assert(offsetof(Record, outgoingTangent) == 24);
        static_assert(offsetof(Record, quadratic) == 36);
        static_assert(offsetof(Record, cubic) == 48);

        bool Admitted(const Words& words) noexcept
        {
            return std::all_of(words.begin(), words.end(), [](std::uint32_t word) {
                const auto magnitude = word & 0x7FFFFFFFu;
                return magnitude == 0 || (magnitude >= 0x1F800000u && magnitude <= 0x45800000u);
            });
        }

        math::Number Widen(std::uint32_t word) noexcept
        {
            float value;
            std::memcpy(&value, &word, sizeof(value));
            return math::From(value);
        }

        math::Number Spill(math::Number value) noexcept
        {
            return math::From(math::Store(value));
        }

        std::uint32_t StoreWord(math::Number value) noexcept
        {
            const auto rounded = math::Store(value);
            std::uint32_t word;
            std::memcpy(&word, &rounded, sizeof(word));
            return word;
        }

        struct Coefficients
        {
            Words quadratic;
            Words cubic;
        };

        Coefficients Calculate(const Record& current, const Record& next) noexcept
        {
            const auto three = math::From(3.0f);
            Coefficients result{};
            for (std::size_t axis = 0; axis < result.quadratic.size(); ++axis)
            {
                const bool z = axis == 2;
                const auto difference = math::Subtract(Widen(next.value[axis]), Widen(current.value[axis]));
                const auto delta = z ? Spill(difference) : difference;
                const auto outgoing = Widen(current.outgoingTangent[axis]);
                const auto incoming = Widen(next.incomingTangent[axis]);

                const auto doubledOutgoing = math::Add(outgoing, outgoing);
                const auto quadraticTangent = math::Add(z ? Spill(doubledOutgoing) : doubledOutgoing, incoming);
                const auto tripledDelta = Spill(math::Multiply(delta, three));
                result.quadratic[axis] = StoreWord(math::Subtract(tripledDelta,
                    z ? quadraticTangent : Spill(quadraticTangent)));

                const auto doubledDelta = math::Add(delta, delta);
                const auto cubicTangent = math::Add(incoming, outgoing);
                result.cubic[axis] = StoreWord(math::Subtract(axis == 0 ? cubicTangent : Spill(cubicTangent),
                    z ? doubledDelta : Spill(doubledDelta)));
            }
            return result;
        }
    }

    bool TryPrepareAnimationVectorCubicCoefficientsPcForAnalysis(
        AnimationVectorCubicPcRecordForAnalysis* keys, std::size_t keyCount) noexcept
    {
        constexpr auto MaximumCount = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
        if (keys == nullptr || keyCount == 0 || keyCount > MaximumCount)
            return false;

        const auto segmentCount = keyCount - 1;
        for (std::size_t i = 0; i < segmentCount; ++i)
        {
            const auto& current = keys[i];
            const auto& next = keys[i + 1];
            if (!Admitted(current.value) || !Admitted(next.value) ||
                !Admitted(current.outgoingTangent) || !Admitted(next.incomingTangent))
                return false;
        }

        for (std::size_t i = 0; i < segmentCount; ++i)
        {
            const auto coefficients = Calculate(keys[i], keys[i + 1]);
            keys[i].quadratic = coefficients.quadratic;
            keys[i].cubic = coefficients.cubic;
        }
        return true;
    }
}
