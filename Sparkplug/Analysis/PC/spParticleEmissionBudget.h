#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    struct ParticleEmissionBudgetInputForAnalysis
    {
        float accumulator=0; // +A8, before maintenance.
        float delta=0, time=0; // Arguments of PC48D210, already split/clamped by caller.
        float lifetimeParameter=1; // +BC, upper bound for accumulator.
        float duration=-1, rate=100; // +B8/+B4.
        bool enabled=true; // +58.
    };
    struct ParticleEmissionBudgetForAnalysis
    {
        bool callProducer=false;
        std::uint32_t requestedCount=0;
        float accumulatorForProducer=0, accumulatorAfter=0;
    };

    // Scalar portion of PC48D210 around the original48C400 call. No particles
    // are allocated here. The producer's returned/actual count is not used by
    // this arithmetic; the qualified full-pool producer leaves +A8 unchanged.
    // Finite inputs, positive lifetime, nonnegative rate and non-wrapping
    // uint32 conversion are host bounds. This is not bit-exact general x87.
    [[nodiscard]] inline bool EvaluateParticleEmissionBudgetForAnalysis(
        const ParticleEmissionBudgetInputForAnalysis& input,
        ParticleEmissionBudgetForAnalysis& output) noexcept
    {
        const auto& p=input;
        for (const float value : {p.accumulator,p.delta,p.time,p.lifetimeParameter,p.duration,p.rate})
            if (!std::isfinite(value)) return false;
        if (!(p.lifetimeParameter>0) || p.rate<0) return false;
        ParticleEmissionBudgetForAnalysis result;
        result.accumulatorForProducer=result.accumulatorAfter=p.accumulator;
        if (!p.enabled || (p.duration>=0 && p.duration<p.time))
        {output=result;return true;}
        const double accumulation=std::min(double(p.accumulator)+p.delta,double(p.lifetimeParameter));
        result.accumulatorForProducer=float(accumulation);
        // Original FST stores float without popping the wider sum: the request
        // uses accumulation, while the subsequent subtraction reads that store.
        const double requested=std::trunc(accumulation*p.rate);
        if (requested<0 || requested>=4294967296.0) return false;
        result.callProducer=true;result.requestedCount=static_cast<std::uint32_t>(requested);
        const double denominator=p.rate>0 ? double(p.rate) : double(0.000001f); // PC35_8637BD.
        result.accumulatorAfter=float(double(result.accumulatorForProducer)-requested/denominator);
        if (!std::isfinite(result.accumulatorForProducer) || !std::isfinite(result.accumulatorAfter)) return false;
        output=result;
        return true;
    }
}
