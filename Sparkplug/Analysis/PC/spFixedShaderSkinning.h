#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Semantic reconstruction of the PC Fixed.rfx weighted vertex expression,
    // not an original CPU method or a shader compiler. See the public contract
    // in docs/engine/animation/skin-deformation.md. GPU instruction scheduling,
    // MAD rounding and rsq precision are not claimed to be bit-identical.
    using FixedSkinVector = std::array<float, 4>;
    // Three shader registers: each is the four coefficients for one output
    // coordinate. This is the uploaded/transposed affine palette, not a native
    // row-major 4x4 matrix or remixapi_Transform reinterpret_cast.
    using FixedSkinMatrix = std::array<FixedSkinVector, 3>;

    // UVTransform is a column-major float3x3 in three float4 registers.
    // Unlike the affine Skin palette above, each register contains a column.
    // Its fourth lane is untouched by the original constant uploader and is
    // not read by Fixed. Input UV.z is replaced with one before multiplication.
    using FixedUvRegistersForAnalysis = std::array<FixedSkinVector, 3>;
    [[nodiscard]] inline bool TransformFixedUvForAnalysis(
        const std::array<float, 2>& input,
        const FixedUvRegistersForAnalysis& columns,
        std::array<float, 3>& output) noexcept
    {
        if (!std::isfinite(input[0]) || !std::isfinite(input[1])) return false;
        std::array<float, 3> result{};
        for (std::size_t coordinate = 0; coordinate < 3; ++coordinate)
        {
            for (const auto& column : columns)
                if (!std::isfinite(column[coordinate])) return false;
            const float vertical = input[1] * columns[1][coordinate];
            const float linear = input[0] * columns[0][coordinate] + vertical;
            result[coordinate] = linear + columns[2][coordinate];
            if (!std::isfinite(result[coordinate])) return false;
        }
        // No division by the third output component is present in the shader.
        // Finite-input/output checks are host guards; refusal preserves output.
        output = result;
        return true;
    }

    struct FixedSkinVertexForAnalysis
    {
        FixedSkinVector position{};
        FixedSkinVector normal{};
        FixedSkinVector weights{};
        std::array<std::uint32_t, 4> indices{};
    };

    struct FixedSkinDeformationForAnalysis
    {
        FixedSkinVector position{};
        FixedSkinVector normal{};
    };

    // Caller supplies already-decoded integral indices and stable palette
    // storage. Finite-input, count/index and zero-normal checks below are host
    // bounds, not new behavior attributed to the original shader. On refusal
    // output is unchanged. The unweighted/fixed-function path is separate.
    [[nodiscard]] inline bool DeformFixedSkinVertexForAnalysis(
        const FixedSkinVertexForAnalysis& input,
        const std::uint32_t weightCount,
        const FixedSkinMatrix* const palette,
        const std::size_t paletteCount,
        FixedSkinDeformationForAnalysis& output) noexcept
    {
        if (weightCount < 1 || weightCount > 4 || !palette
            || paletteCount < 1 || paletteCount > 16)
        {
            return false;
        }
        for (std::size_t component = 0; component < 4; ++component)
        {
            if (!std::isfinite(input.position[component])
                || !std::isfinite(input.normal[component]))
            {
                return false;
            }
        }

        FixedSkinDeformationForAnalysis result{};
        result.position[3] = input.position[3];
        result.normal[3] = input.normal[3];
        for (std::uint32_t influence = 0; influence < weightCount; ++influence)
        {
            const auto index = input.indices[influence];
            const auto weight = input.weights[influence];
            if (index >= paletteCount || !std::isfinite(weight))
            {
                return false;
            }
            for (std::size_t coordinate = 0; coordinate < 3; ++coordinate)
            {
                const auto& coefficients = palette[index][coordinate];
                for (const auto coefficient : coefficients)
                {
                    if (!std::isfinite(coefficient))
                    {
                        return false;
                    }
                }
                const float position = input.position[0] * coefficients[0]
                    + input.position[1] * coefficients[1]
                    + input.position[2] * coefficients[2]
                    + input.position[3] * coefficients[3];
                const float normal = input.normal[0] * coefficients[0]
                    + input.normal[1] * coefficients[1]
                    + input.normal[2] * coefficients[2];
                // All authored weights participate, including zero/negative
                // values and the last weight. No implicit remainder or sum
                // normalization is performed by Fixed.rfx.
                result.position[coordinate] += position * weight;
                result.normal[coordinate] += normal * weight;
            }
        }

        float squaredLength = 0;
        for (std::size_t component = 0; component < 4; ++component)
        {
            if (!std::isfinite(result.position[component])
                || !std::isfinite(result.normal[component]))
            {
                return false;
            }
            squaredLength += result.normal[component] * result.normal[component];
        }
        if (!(squaredLength > 0) || !std::isfinite(squaredLength))
        {
            return false;
        }
        const float length = std::sqrt(squaredLength);
        for (auto& component : result.normal)
        {
            component /= length;
        }
        output = result;
        return true;
    }
}
