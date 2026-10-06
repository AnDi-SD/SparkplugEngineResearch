#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Raw x86 evidence only; pointers are target 32-bit words, never host ones.
    struct LightController final
    {
        std::array<std::uint32_t, 4> baseObject;
        std::uint8_t enabled;
        std::array<std::uint8_t, 3> padding11;
        std::uint32_t nextController;
        std::uint32_t previousController;
        std::array<std::uint32_t, 4> colorEvalBase;
        std::uint32_t color1ARGB;
        std::uint32_t color2ARGB;
        std::array<std::uint32_t, 4> functionEvalBase;
        float time;
        float frequency;
        float reciprocal;
        float amplitude;
        float xOffset;
        float yOffset;
        float pitch;
        float clampLimit;
        std::uint8_t clampEnabled;
        std::array<std::uint8_t, 3> padding65;
        std::uint32_t functionType;
        std::uint32_t borrowedLight;
    };
    static_assert(sizeof(LightController) == 0x70);
    static_assert(offsetof(LightController, colorEvalBase) == 0x1C);
    static_assert(offsetof(LightController, color1ARGB) == 0x2C);
    static_assert(offsetof(LightController, functionEvalBase) == 0x34);
    static_assert(offsetof(LightController, time) == 0x44);
    static_assert(offsetof(LightController, functionType) == 0x68);
    static_assert(offsetof(LightController, borrowedLight) == 0x6C);
}
