#pragma once

// Analytical names for PC463670 (sphere union), 469820 (cached matrix sphere),
// 4250F0 (PRS sphere), 461EB0 (inverse PRS) and 424840 (six-plane rejection).
// Finite-input reconstruction, not a bit-identical x87 or original-symbol claim.
#include "spNodeTransformMath.h"
#include <algorithm>

namespace sparkplug::evidence::pc::render_node_math
{
    using Sphere = std::array<float, 4>;
    using Planes = std::array<Sphere, 6>;
    using node_math::Matrix3;
    using node_math::Matrix4;
    using node_math::Vector3;
    inline constexpr float Epsilon = 0.001F;
    inline constexpr Matrix4 Identity4{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    template <std::size_t N> inline bool Finite(const std::array<float, N>& values) noexcept
    {
        for (const auto value : values)
            if (!std::isfinite(value))
                return false;
        return true;
    }

    inline void Merge(Sphere& target, const Sphere& other) noexcept
    {
        const Vector3 delta{other[0] - target[0], other[1] - target[1], other[2] - target[2]};
        const float distance =
            std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
        if (other[3] < Epsilon || distance + other[3] < target[3])
            return;
        if (target[3] < Epsilon || distance + target[3] < other[3])
        {
            target = other;
            return;
        }
        // Native439290 compares each center coordinate against the tolerance,
        // not the length. Near-coincident centers keep target's old center.
        if (std::fabs(delta[0]) > Epsilon || std::fabs(delta[1]) > Epsilon ||
            std::fabs(delta[2]) > Epsilon)
            for (std::size_t i = 0; i < 3; ++i)
                target[i] =
                    (target[i] + other[i] + delta[i] / distance * (other[3] - target[3])) * .5F;
        target[3] = (target[3] + other[3] + distance) * .5F;
    }

    inline Sphere FromCachedMatrix(const Sphere& local, const Matrix4& matrix) noexcept
    {
        Sphere result{};
        for (std::size_t c = 0; c < 3; ++c)
            result[c] = local[0] * matrix[c] + local[1] * matrix[4 + c] + local[2] * matrix[8 + c] +
                        matrix[12 + c];
        // The original helper measures the transformed +X radius point. It
        // does NOT use max-scale here; the PRS dirty branch below does that.
        Vector3 delta{};
        for (std::size_t c = 0; c < 3; ++c)
            delta[c] = (local[0] + local[3]) * matrix[c] + local[1] * matrix[4 + c] +
                       local[2] * matrix[8 + c] + matrix[12 + c] - result[c];
        result[3] = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
        return result;
    }

    inline Sphere FromPRS(const Sphere& local, const Vector3& position, const Matrix3& orientation,
                          const Vector3& scale) noexcept
    {
        auto center = node_math::Transform(
            {local[0] * scale[0], local[1] * scale[1], local[2] * scale[2]}, orientation);
        return {center[0] + position[0], center[1] + position[1], center[2] + position[2],
                local[3] *
                    std::max({std::fabs(scale[0]), std::fabs(scale[1]), std::fabs(scale[2])})};
    }

    inline Matrix4 InversePRS(const Vector3& position, const Matrix3& orientation,
                              const Vector3& inverseScale) noexcept
    {
        Matrix4 result{};
        // Literal native inverse-PRS composition: transpose orientation, then
        // reciprocal scales. Not a general inverse for a sheared matrix.
        for (std::size_t c = 0; c < 3; ++c)
        {
            for (std::size_t r = 0; r < 3; ++r)
                result[r * 4 + c] = orientation[c * 3 + r] * inverseScale[c];
            result[12 + c] =
                -(position[0] * orientation[c * 3] + position[1] * orientation[c * 3 + 1] +
                  position[2] * orientation[c * 3 + 2]) *
                inverseScale[c];
        }
        result[15] = 1;
        return result;
    }

    inline bool Culled(const Sphere& sphere, const Planes& planes, bool bypass) noexcept
    {
        if (bypass || sphere[3] <= Epsilon)
            return false;
        for (const auto& plane : planes)
            if (plane[0] * sphere[0] + plane[2] * sphere[2] + plane[1] * sphere[1] - plane[3] <
                -sphere[3])
                return true;
        return false;
    }
} // namespace sparkplug::evidence::pc::render_node_math
