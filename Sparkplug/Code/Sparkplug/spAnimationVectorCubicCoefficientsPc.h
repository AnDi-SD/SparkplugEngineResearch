#pragma once

#include "spAnimationVectorCubicPc.h"

#include <cstddef>

namespace sparkplug::reconstruction
{
    // PC 0x00493290 with round-to-nearest binary80 arithmetic. The caller
    // supplies an array of 60-byte packed cubic keys. Only coefficients of
    // segments with a following key are written. Refusal preserves all keys.
    [[nodiscard]] bool TryPrepareAnimationVectorCubicCoefficientsPcForAnalysis(
        AnimationVectorCubicPcRecordForAnalysis* keys, std::size_t keyCount) noexcept;
}
