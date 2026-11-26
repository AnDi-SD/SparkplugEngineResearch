#pragma once

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    struct FunctionEvalLinearRampPcInputForAnalysis
    {
        std::uint32_t storedTimeBits;
        std::uint32_t pitchBits;
        std::uint32_t yOffsetBits;
        std::uint32_t clampLimitBits;
        std::uint8_t clampEnabled;
    };

    using FunctionEvalLinearRampPc80ForAnalysis = std::array<std::uint8_t, 10>;

    // PC type 8 suffix at 004787BC with control word 037F. Time is the
    // already-stored binary32 word. All four floats are normal or signed
    // zero with magnitude at most 16384. The result is the physical
    // little-endian binary80 ST0 value. A refusal preserves the output.
    [[nodiscard]] bool TryEvaluateFunctionEvalLinearRampPcForAnalysis(
        const FunctionEvalLinearRampPcInputForAnalysis& input,
        FunctionEvalLinearRampPc80ForAnalysis& output) noexcept;
}
