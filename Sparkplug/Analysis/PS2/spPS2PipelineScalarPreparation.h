#pragma once

// Static reconstruction of the bounded EE helper at 0x002076E0.
// Names and this captured-value interface are analytical, not native C++ ABI.
// See docs/engine/rendering/ps2-cpu-preparation.md. This is not an EE FPU model.

#include <array>
#include <cstdint>

namespace sparkplug::evidence::ps2
{
    using PipelineScalarWordVectorForAnalysis = std::array<std::uint32_t, 4>;

    // Captured words from one original input object, not its memory layout.
    // Retaining words makes the three copied lanes independent of host floats.
    struct PipelineScalarInputForAnalysis final
    {
        std::array<std::uint32_t, 3> field70{}; // Native float words +0x70/+74/+78.
        PipelineScalarWordVectorForAnalysis fieldD0{}; // +0xD0/+D4/+D8/+DC.
        std::uint32_t fieldEC = 0; // Native divisor word +0xEC.
    };

    // Arithmetic supplies DivideSingle(numeratorBits, denominatorBits) and
    // MultiplySingle(firstBits, secondBits), each returning a raw single word.
    // Its rounding, encoded special values and status behavior are a separate
    // qualification. No default IEEE arithmetic silently substitutes for EE.
    // Use stable captured inputs and valid, non-overlapping output/counter
    // storage. The provider must not mutate those inputs/outputs or reenter.
    // This interface does not model arbitrary native pointer aliases/faults.
    template<class Arithmetic>
    inline void PreparePipelineScalarsForAnalysis(
        const std::array<const PipelineScalarInputForAnalysis*, 4>& inputs,
        std::array<PipelineScalarWordVectorForAnalysis, 4>& firstStream,
        std::array<PipelineScalarWordVectorForAnalysis, 4>& secondStream,
        std::uint32_t& accumulatedCount,
        std::uint32_t& mirroredCount,
        Arithmetic& arithmetic)
    {
        constexpr std::uint32_t minusOneSingle = 0xBF800000U;
        constexpr std::uint32_t scale255Single = 0x437F0000U;
        for (unsigned index = 0; index < inputs.size(); ++index)
        {
            const auto* input = inputs[index];
            if (input == nullptr) break; // A NULL terminates; later slots are not visited.
            const auto captured = *input;
            const auto inverse = arithmetic.DivideSingle(minusOneSingle, captured.fieldEC);
            firstStream[index][0] = captured.field70[0];
            firstStream[index][1] = captured.field70[1];
            firstStream[index][2] = captured.field70[2];
            firstStream[index][3] = inverse;
            for (unsigned component = 0; component < 4; ++component)
                secondStream[index][component] = arithmetic.MultiplySingle(
                    scale255Single, captured.fieldD0[component]);
            accumulatedCount += std::uint32_t{1}; // Native ADDIU/SW wraps the low32 bits.
        }
        mirroredCount = accumulatedCount; // Native JR delay slot, including first-slot NULL.
    }
}
