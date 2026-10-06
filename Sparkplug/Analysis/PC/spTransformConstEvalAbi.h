#pragma once

// Observed32-bit PC allocation. Portable class storage is independent.
#include "spUVFunctionAbi.h"

namespace sparkplug::evidence::pc
{
    struct spTransformConstEvalObservedLayout final
    {
        std::array<std::uint8_t, 0x10> transformEvaluatorBase;
        std::array<float, 3> velocity;
        std::array<float, 3> axis;
        float angle;
        spFunctionEvalObservedLayout scale;
        std::uint8_t positionEnabled, rotationEnabled;
        std::array<std::uint8_t, 2> untouchedPadding;
    };
    static_assert(sizeof(spTransformConstEvalObservedLayout) == 0x68);
    static_assert(offsetof(spTransformConstEvalObservedLayout, velocity) == 0x10);
    static_assert(offsetof(spTransformConstEvalObservedLayout, axis) == 0x1C);
    static_assert(offsetof(spTransformConstEvalObservedLayout, angle) == 0x28);
    static_assert(offsetof(spTransformConstEvalObservedLayout, scale) == 0x2C);
    static_assert(offsetof(spTransformConstEvalObservedLayout, positionEnabled) == 0x64);
    static_assert(offsetof(spTransformConstEvalObservedLayout, rotationEnabled) == 0x65);
    static_assert(offsetof(spTransformConstEvalObservedLayout, scale) +
        offsetof(spFunctionEvalObservedLayout, type) == 0x60);
    inline constexpr Address32 spTransformConstEvalFactory = 0x601CB0;
    inline constexpr Address32 spTransformConstEvalEvaluate = 0x601B20;
    inline constexpr Address32 spTransformConstEvalClone = 0x601D10;
    inline constexpr Address32 spTransformConstEvalDelete = 0x601C90;
}
