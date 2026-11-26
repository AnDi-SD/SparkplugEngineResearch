#pragma once

#include <cstdint>

namespace sparkplug::reconstruction
{
    struct ParticleEmissionBudgetPcInputForAnalysis
    {
        std::uint32_t accumulatorBits;
        std::uint32_t deltaBits;
        std::uint32_t timeBits;
        std::uint32_t lifetimeCeilingBits;
        std::uint32_t durationBits;
        std::uint32_t rateBits;
        std::uint8_t enabled;
    };

    struct ParticleEmissionBudgetPcForAnalysis
    {
        bool reachesProducer;
        std::uint32_t requested;
        std::uint32_t provisionalAccumulatorBits;
        std::uint32_t accumulatorAtProducerBits;
        std::uint32_t accumulatorAfterDebitBits;
    };

    // PC 0048D275 and 0048D2ED under FPCW=037F. The supplied raw floats are
    // finite normals or +0, bounded by abs(value) <= 2^31. Delta, time and rate
    // are nonnegative, the lifetime ceiling positive, and truncation must
    // yield 0..INT32_MAX. The debit uses the prepare phase's unchanged state.
    // Skips report the original accumulator in all three result positions.
    // A refusal preserves output.
    [[nodiscard]] bool TryEvaluateParticleEmissionBudgetPcNearestForAnalysis(
        const ParticleEmissionBudgetPcInputForAnalysis& input, ParticleEmissionBudgetPcForAnalysis& output) noexcept;
}
