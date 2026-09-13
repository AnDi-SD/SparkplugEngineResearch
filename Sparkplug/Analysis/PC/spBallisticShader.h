#pragma once

#include <array>
#include <cmath>

namespace sparkplug::evidence::pc
{
    // Semantic reconstruction of BallisticPFX.vsh expressions. This is not
    // the original particle-system CPU class or a replacement renderer.
    // Instruction precision (MAD/rcp/rsq), clipping and point rasterization
    // remain separate. See docs/engine/effects/ballistic-shader.md.
    using BallisticVector = std::array<float, 4>;

    struct BallisticShaderInputForAnalysis
    {
        BallisticVector position{}; // v0; w participates only in point distance
        BallisticVector velocity{}; // v1.xyz, despite the NORMAL declaration
        float birthTime = 0;        // v2.x
        float lifetime = 1;         // v2.y
    };

    struct BallisticShaderParametersForAnalysis
    {
        std::array<BallisticVector, 4> viewProjection{}; // c0-c3, uploaded order
        BallisticVector accelerationBegin{}; // c4.xyz
        BallisticVector accelerationEnd{};   // c5.xyz
        BallisticVector cameraPosition{};    // c6, all four components
        BallisticVector timeLoopScales{};    // c7: time, loop, begin, end
        float framebufferWidth = 1;         // c8.x
        BallisticVector colorBegin{};        // c9
        BallisticVector colorEnd{};          // c10
    };

    struct BallisticShaderOutputForAnalysis
    {
        std::array<float, 3> position{};
        BallisticVector clipPosition{};
        BallisticVector color{}; // oD0 expression before output conversion
        float age = 0;
        float normalizedAge = 0;
        float pointSize = 0; // oPts expression before rasterizer limits
    };

    // Finite-input/output, positive-lifetime and nonzero-distance restrictions
    // are our analysis boundary, not guards attributed to the game shader.
    // Refusal leaves output unchanged; out-of-lifetime particles are accepted.
    [[nodiscard]] inline bool EvaluateBallisticShaderForAnalysis(
        const BallisticShaderInputForAnalysis& input,
        const BallisticShaderParametersForAnalysis& parameters,
        BallisticShaderOutputForAnalysis& output) noexcept
    {
        const auto finite = [](const BallisticVector& vector)
        {
            for (const float value : vector) if (!std::isfinite(value)) return false;
            return true;
        };
        if (!finite(input.position) || !finite(input.velocity)
            || !std::isfinite(input.birthTime) || !std::isfinite(input.lifetime)
            || !(input.lifetime > 0) || !finite(parameters.accelerationBegin)
            || !finite(parameters.accelerationEnd) || !finite(parameters.cameraPosition)
            || !finite(parameters.timeLoopScales) || !finite(parameters.colorBegin)
            || !finite(parameters.colorEnd) || !std::isfinite(parameters.framebufferWidth))
            return false;
        for (const auto& row : parameters.viewProjection) if (!finite(row)) return false;

        BallisticShaderOutputForAnalysis result{};
        result.age = parameters.timeLoopScales[0] - input.birthTime;
        if (result.age >= input.lifetime && parameters.timeLoopScales[1] >= 0.5f)
            result.age -= input.lifetime; // Exactly one subtraction, not modulo.
        const float squaredAge = result.age * result.age;
        result.normalizedAge = result.age * (1.0f / input.lifetime);
        for (unsigned coordinate = 0; coordinate < 3; ++coordinate)
        {
            const float linear = result.age * input.velocity[coordinate] + input.position[coordinate];
            const float quadratic = squaredAge * parameters.accelerationBegin[coordinate];
            const float accelerationDelta = parameters.accelerationBegin[coordinate]
                - parameters.accelerationEnd[coordinate];
            const float cubic = (squaredAge * accelerationDelta) * result.normalizedAge;
            // The shader constant is decimal 0.166667, not an exact 1/6.
            result.position[coordinate] = cubic * 0.166667f + (quadratic * 0.5f + linear);
            if (!std::isfinite(result.position[coordinate])) return false;
        }
        float squaredDistance = 0;
        for (unsigned component = 0; component < 4; ++component)
        {
            result.clipPosition[component] = result.position[0] * parameters.viewProjection[0][component]
                + result.position[1] * parameters.viewProjection[1][component]
                + result.position[2] * parameters.viewProjection[2][component]
                + parameters.viewProjection[3][component]; // Implicit homogeneous 1.
            const float delta = input.position[component] - parameters.cameraPosition[component];
            squaredDistance += delta * delta; // Original position and dp4.
            result.color[component] = result.normalizedAge * parameters.colorEnd[component]
                + (1.0f - result.normalizedAge) * parameters.colorBegin[component];
        }
        if (!(squaredDistance > 0) || !std::isfinite(squaredDistance)) return false;
        const float size = (1.0f - result.normalizedAge) * parameters.timeLoopScales[2]
            + result.normalizedAge * parameters.timeLoopScales[3];
        const float lowerMask = result.normalizedAge >= 0 ? 1.0f : 0.0f;
        const float upperMask = result.normalizedAge <= 1 ? 1.0f : 0.0f;
        result.pointSize = (1.0f / std::sqrt(squaredDistance))
            * (((size * lowerMask) * upperMask) * parameters.framebufferWidth);
        if (!std::isfinite(result.age) || !std::isfinite(result.normalizedAge)
            || !finite(result.clipPosition) || !finite(result.color) || !std::isfinite(result.pointSize))
            return false;
        output = result;
        return true;
    }
}
