#include "spParticleEmissionBudgetPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cstring>
#include <initializer_list>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        constexpr std::uint32_t MaxMagnitude = 0x4F000000u;
        constexpr std::uint32_t ZeroRateDivisor = 0x358637BDu;

        float FloatFromBits(std::uint32_t bits) noexcept
        {
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }

        math::Number FromBits(std::uint32_t bits) noexcept
        {
            return math::From(FloatFromBits(bits));
        }

        std::uint32_t StoreBits(math::Number number) noexcept
        {
            const auto value = math::Store(number);
            std::uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        }

        bool Admitted(const ParticleEmissionBudgetPcInputForAnalysis& input) noexcept
        {
            for (const auto bits : {input.accumulatorBits, input.deltaBits, input.timeBits,
                    input.lifetimeCeilingBits, input.durationBits, input.rateBits})
            {
                const auto magnitude = bits & 0x7FFFFFFFu;
                if (bits != 0 && (magnitude < 0x00800000u || magnitude > MaxMagnitude))
                    return false;
            }
            return input.deltaBits <= MaxMagnitude && input.timeBits <= MaxMagnitude && input.rateBits <= MaxMagnitude
                && input.lifetimeCeilingBits != 0 && input.lifetimeCeilingBits <= MaxMagnitude;
        }

        bool TruncateRequest(math::Number quota, std::uint32_t& requested) noexcept
        {
            if (quota.mantissa == 0 || quota.exponent <= -64)
            {
                requested = 0;
                return true;
            }
            // A normalized 64-bit significand at exponent -32 is at least 2^31.
            if (quota.negative || quota.exponent >= -32)
                return false;
            requested = static_cast<std::uint32_t>(quota.mantissa >> -quota.exponent);
            return true;
        }

        bool Prepare(const ParticleEmissionBudgetPcInputForAnalysis& input,
            ParticleEmissionBudgetPcForAnalysis& evaluation) noexcept
        {
            if (input.enabled == 0)
                return true;
            const auto duration = FromBits(input.durationBits);
            if (!duration.negative && math::nearest::Compare(duration, FromBits(input.timeBits)) < 0)
                return true;

            const auto combined = math::Add(FromBits(input.deltaBits), FromBits(input.accumulatorBits));
            evaluation.provisionalAccumulatorBits = StoreBits(combined);
            const auto ceiling = FromBits(input.lifetimeCeilingBits);
            const auto prepared = math::nearest::Compare(combined, ceiling) > 0 ? ceiling : combined;
            evaluation.accumulatorAtProducerBits = StoreBits(prepared);
            // The FSTs leave the extended values available to the quota product.
            if (!TruncateRequest(math::Multiply(prepared, FromBits(input.rateBits)), evaluation.requested))
                return false;
            evaluation.reachesProducer = true;
            return true;
        }

        std::uint32_t Debit(std::uint32_t preparedAccumulatorBits, std::uint32_t rateBits,
            std::uint32_t requested) noexcept
        {
            const auto divisor = FloatFromBits(rateBits == 0 ? ZeroRateDivisor : rateBits);
            const auto request = math::nearest::NormalizeNearest(math::Wide::Shifted(requested, 0), 0, false);
            const auto quotient = math::nearest::DividePositiveFloatNearest(request, divisor);
            return StoreBits(math::Subtract(FromBits(preparedAccumulatorBits), quotient));
        }
    }

    bool TryEvaluateParticleEmissionBudgetPcNearestForAnalysis(const ParticleEmissionBudgetPcInputForAnalysis& input,
        ParticleEmissionBudgetPcForAnalysis& output) noexcept
    {
        if (!Admitted(input))
            return false;

        ParticleEmissionBudgetPcForAnalysis evaluation{false, 0, input.accumulatorBits, input.accumulatorBits,
            input.accumulatorBits};
        if (!Prepare(input, evaluation))
            return false;
        if (evaluation.reachesProducer)
            evaluation.accumulatorAfterDebitBits = Debit(evaluation.accumulatorAtProducerBits, input.rateBits,
                evaluation.requested);
        output = evaluation;
        return true;
    }
}
